"""Rebuild the city terrain's local mesh input from Landscape height exports.

This runs with ordinary Python, NumPy and Pillow. It does not import Unreal or
modify any project asset. The resulting mesh.json is an input to the separate
UE Geometry Script authoring step.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path

import numpy as np
from PIL import Image


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(data, stream, ensure_ascii=False)
    os.replace(temporary, path)


def _number(value, name):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("%s must be a finite number" % name)
    result = float(value)
    if not math.isfinite(result):
        raise ValueError("%s must be a finite number" % name)
    return result


def _int(value, name):
    number = _number(value, name)
    if not number.is_integer():
        raise ValueError("%s must be an integer" % name)
    return int(number)


def _axis(start, end, step):
    values = np.arange(start, end + 1, step, dtype=np.int32)
    if values[-1] != end:
        values = np.append(values, np.int32(end))
    return values


def _source_metadata(data, report_path):
    if data.get("success") is not True:
        raise ValueError("height export did not report success")
    world = data.get("world")
    parent = data.get("parent")
    proxies = data.get("proxies")
    if not isinstance(world, str) or not world.startswith("/Game/"):
        raise ValueError("height export world is missing or invalid")
    if not isinstance(parent, dict) or not isinstance(proxies, list) or not proxies:
        raise ValueError("height export needs a parent and proxy list")
    if not isinstance(parent.get("path"), str) or not parent["path"].startswith(world + ":"):
        raise ValueError("Landscape parent does not belong to the exported world")
    location, scale = parent.get("location"), parent.get("scale")
    if not isinstance(location, list) or len(location) != 3 or not isinstance(scale, list) or len(scale) != 3:
        raise ValueError("Landscape transform is incomplete")
    location = [_number(v, "parent.location") for v in location]
    scale = [_number(v, "parent.scale") for v in scale]
    yaw = _number(parent.get("yaw"), "parent.yaw")
    if min(scale) <= 0 or not math.isclose(scale[0], scale[1], rel_tol=0, abs_tol=1e-5):
        raise ValueError("expected positive, equal Landscape X/Y scales")
    if not -360 <= yaw <= 360:
        raise ValueError("Landscape yaw is outside the expected range")

    entries = []
    labels = set()
    for index, raw in enumerate(proxies):
        if not isinstance(raw, dict):
            raise ValueError("proxy %d metadata is invalid" % index)
        label, path = raw.get("label"), raw.get("file")
        if not isinstance(label, str) or not label or label in labels:
            raise ValueError("proxy labels must be unique and nonempty")
        labels.add(label)
        if not isinstance(path, str) or not path:
            raise ValueError("proxy %s has no PNG file" % label)
        image_path = Path(path)
        if not image_path.is_absolute():
            image_path = report_path.parent / image_path
        if not image_path.is_file():
            raise FileNotFoundError("proxy %s PNG missing: %s" % (label, image_path))
        bounds = [_int(raw.get(key), "%s.%s" % (label, key))
                  for key in ("minx", "miny", "maxx", "maxy")]
        minx, miny, maxx, maxy = bounds
        if maxx <= minx or maxy <= miny:
            raise ValueError("proxy %s has empty bounds" % label)
        size = _int(raw.get("component_size_quads"), label + ".component_size_quads")
        bases = raw.get("section_bases")
        if size <= 0 or not isinstance(bases, list) or not bases:
            raise ValueError("proxy %s needs component size and section bases" % label)
        seen_bases = set()
        for base in bases:
            if not isinstance(base, list) or len(base) != 2:
                raise ValueError("proxy %s has invalid section base" % label)
            bx, by = (_int(base[0], "section x"), _int(base[1], "section y"))
            if (bx, by) in seen_bases or not (minx <= bx and bx + size <= maxx and
                                             miny <= by and by + size <= maxy):
                raise ValueError("proxy %s has duplicate/out-of-range section base" % label)
            seen_bases.add((bx, by))
        entries.append((raw, image_path, bounds, size, seen_bases))
    return world, parent, location, yaw, scale, entries


def build(export_report, mesh_json, report_json, step_quads=8, waterline_local_cm=-2000):
    """Stitch proxy PNGs, then reconstruct local vertices, UVs and triangles.

    Output paths are explicit. Invalid exports raise before the mesh is written.
    The result describes the original Landscape transform but does not apply it:
    the UE StaticMeshActor carries that transform when the mesh is placed.
    """
    export_path = Path(export_report).resolve()
    mesh_path = Path(mesh_json).resolve()
    validation_path = Path(report_json).resolve()
    if not export_path.is_file():
        raise FileNotFoundError("height export report missing: %s" % export_path)
    if len({export_path, mesh_path, validation_path}) != 3:
        raise ValueError("export report, mesh JSON and validation JSON must differ")
    step = _int(step_quads, "step_quads")
    waterline = _number(waterline_local_cm, "waterline_local_cm")
    if step <= 0 or not -100000 < waterline < 100000:
        raise ValueError("invalid terrain sampling step or waterline")
    data = json.loads(export_path.read_text(encoding="utf-8"))
    world, parent, location, yaw, scale, entries = _source_metadata(data, export_path)
    minx = min(bounds[0] for _, _, bounds, _, _ in entries)
    miny = min(bounds[1] for _, _, bounds, _, _ in entries)
    maxx = max(bounds[2] for _, _, bounds, _, _ in entries)
    maxy = max(bounds[3] for _, _, bounds, _, _ in entries)
    if (maxx - minx) * (maxy - miny) > 100_000_000:
        raise ValueError("export extent exceeds the expected terrain size")
    heights = np.full((maxy - miny + 1, maxx - minx + 1), -1, dtype=np.int32)
    proxy_reports = []
    for raw, image_path, bounds, component_size, bases in entries:
        x0, y0, x1, y1 = bounds
        with Image.open(image_path) as image:
            if image.mode != "RGBA":
                raise ValueError("proxy %s must be an RGBA PNG" % raw["label"])
            if image.width < x1 - x0 + 1 or image.height < y1 - y0 + 1:
                raise ValueError("proxy %s image is smaller than its extent" % raw["label"])
            pixels = np.asarray(image.crop((0, 0, x1 - x0 + 1, y1 - y0 + 1)))
            image_size = [image.width, image.height]
        alpha = pixels[:, :, 3]
        if np.any((alpha != 0) & (alpha != 255)):
            raise ValueError("proxy %s export has unexpected alpha values" % raw["label"])
        valid = alpha == 0
        if not np.any(valid):
            raise ValueError("proxy %s export contains no valid height pixels" % raw["label"])
        allowed = np.zeros(valid.shape, dtype=bool)
        for bx, by in bases:
            allowed[by - y0:by - y0 + component_size + 1,
                    bx - x0:bx - x0 + component_size + 1] = True
        if np.any(valid & ~allowed):
            raise ValueError("proxy %s has pixels outside its reported components" % raw["label"])
        values = pixels[:, :, 0].astype(np.int32) * 256 + pixels[:, :, 1].astype(np.int32)
        if np.max(values[valid]) == 0:
            raise ValueError("proxy %s export has all-zero heights; render target may be unready" % raw["label"])
        destination = heights[y0 - miny:y1 - miny + 1, x0 - minx:x1 - minx + 1]
        overlap = valid & (destination >= 0)
        if np.any(destination[overlap] != values[overlap]):
            raise ValueError("proxy %s height disagrees with an overlapping proxy" % raw["label"])
        destination[valid] = values[valid]
        proxy_reports.append({
            "label": raw["label"], "file": str(image_path), "sha256": _sha256(image_path),
            "image_size": image_size, "bounds_quads": bounds,
            "component_count": len(bases), "valid_pixels": int(np.count_nonzero(valid)),
            "empty_pixels_in_extent": int(valid.size - np.count_nonzero(valid)),
            "overlap_pixels": int(np.count_nonzero(overlap)),
            "height_min": int(np.min(values[valid])), "height_max": int(np.max(values[valid])),
        })
    if np.count_nonzero(heights >= 0) == 0:
        raise ValueError("stitched grid has no valid heights")

    xs = _axis(minx, maxx, step)
    ys = _axis(miny, maxy, step)
    sample_heights = heights[np.ix_(ys - miny, xs - minx)]
    z = np.where(sample_heights < 0, waterline,
                 (sample_heights - 32768) * scale[2] / 128.0)
    if not np.all(np.isfinite(z)):
        raise ValueError("sampled terrain contains invalid heights")
    top_left = z[:-1, :-1]
    top_right = z[:-1, 1:]
    bottom_left = z[1:, :-1]
    bottom_right = z[1:, 1:]
    first = np.maximum.reduce((top_left, top_right, bottom_right)) >= waterline
    second = np.maximum.reduce((top_left, bottom_right, bottom_left)) >= waterline
    if not np.any(first) or not np.any(second):
        raise ValueError("sampled terrain contains no surface triangles")
    active = np.zeros(z.shape, dtype=bool)
    active[:-1, :-1] |= first | second
    active[:-1, 1:] |= first
    active[1:, 1:] |= first | second
    active[1:, :-1] |= second
    rows, columns = np.nonzero(active)
    ids = np.full(z.shape, -1, dtype=np.int32)
    ids[rows, columns] = np.arange(len(rows), dtype=np.int32)
    vertices = [[round(int(xs[c]) * scale[0], 1),
                 round(int(ys[r]) * scale[1], 1), round(float(z[r, c]), 1)]
                for r, c in zip(rows, columns)]
    uv = [[round((int(xs[c]) - minx) / (maxx - minx), 5),
           round((int(ys[r]) - miny) / (maxy - miny), 5)]
          for r, c in zip(rows, columns)]
    triangles = []
    for selected, corners in ((first, ((0, 0), (0, 1), (1, 1))),
                              (second, ((0, 0), (1, 1), (1, 0)))):
        for row, column in np.argwhere(selected):
            triangles.append([int(ids[row + dr, column + dc]) for dr, dc in corners])
    if any(min(triangle) < 0 or max(triangle) >= len(vertices) for triangle in triangles):
        raise RuntimeError("triangle references an unselected vertex")

    mesh = {
        "vertices": vertices, "uv": uv, "triangles": triangles,
        "actor": {"loc": location, "yaw": yaw},
    }
    _write_json(mesh_path, mesh)
    radians = math.radians(yaw)
    cosine, sine = math.cos(radians), math.sin(radians)
    points = np.asarray(vertices, dtype=np.float64)
    world_x = location[0] + cosine * points[:, 0] - sine * points[:, 1]
    world_y = location[1] + sine * points[:, 0] + cosine * points[:, 1]
    report = {
        "status": "built", "world": world, "landscape_path": parent["path"],
        "export_report": str(export_path), "export_report_sha256": _sha256(export_path),
        "mesh_json": str(mesh_path), "mesh_json_sha256": _sha256(mesh_path),
        "landscape_transform": {"location": location, "yaw": yaw, "scale": scale},
        "extent_quads": [minx, miny, maxx, maxy],
        "sample_step_quads": step, "sample_grid_size": [len(xs), len(ys)],
        "waterline_local_cm": waterline,
        "height_pixels_valid": int(np.count_nonzero(heights >= 0)),
        "height_pixels_missing": int(np.count_nonzero(heights < 0)),
        "height_min": int(np.min(heights[heights >= 0])),
        "height_max": int(np.max(heights)),
        "sampled_valid": int(np.count_nonzero(sample_heights >= 0)),
        "sampled_missing": int(np.count_nonzero(sample_heights < 0)),
        "vertices": len(vertices), "triangles": len(triangles),
        "triangle_pass_counts": [int(np.count_nonzero(first)), int(np.count_nonzero(second))],
        "local_vertex_bounds_cm": [points.min(axis=0).tolist(), points.max(axis=0).tolist()],
        "world_xy_bounds_cm": [[float(world_x.min()), float(world_y.min())],
                                [float(world_x.max()), float(world_y.max())]],
        "proxies": proxy_reports,
    }
    _write_json(validation_path, report)
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("export_report", help="height_export.json from the UE export step")
    parser.add_argument("mesh_json", help="output mesh.json for Geometry Script")
    parser.add_argument("report_json", help="output validation report")
    parser.add_argument("--step-quads", type=int, default=8)
    parser.add_argument("--waterline-local-cm", type=float, default=-2000)
    args = parser.parse_args()
    result = build(args.export_report, args.mesh_json, args.report_json,
                   args.step_quads, args.waterline_local_cm)
    print("%d vertices, %d triangles: %s" %
          (result["vertices"], result["triangles"], result["mesh_json"]))
