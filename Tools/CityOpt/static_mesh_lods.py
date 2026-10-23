"""Author only the StaticMesh LODs named in an explicit Saved JSON manifest.

Call run(manifest_path, "inspect" | "apply" | "verify") from UE5.4 editor Python.
This module never searches assets or changes the current map.
"""

import hashlib
import json
import math
import os
import re
import shutil
import traceback

import unreal


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _saved_path(path, label):
    if not isinstance(path, str) or not os.path.isabs(path):
        raise ValueError("%s must be an absolute path under project Saved" % label)
    result = os.path.normcase(os.path.realpath(path))
    saved = os.path.normcase(os.path.realpath(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())))
    if os.path.commonpath((result, saved)) != saved:
        raise ValueError("%s must stay under project Saved: %s" % (label, path))
    return result


def _asset_path(path):
    if not isinstance(path, str):
        raise ValueError("expected exact /Game StaticMesh package path: %r" % path)
    if "." in path:
        package, object_name = path.rsplit(".", 1)
        if object_name != package.rsplit("/", 1)[-1]:
            raise ValueError("asset object name does not match package: %s" % path)
        path = package
    if not re.fullmatch(r"/Game/[A-Za-z0-9_/-]+", path):
        raise ValueError("expected exact /Game StaticMesh package path: %r" % path)
    return path


def _source_file(asset_path):
    relative = asset_path[len("/Game/"):].replace("/", os.sep) + ".uasset"
    return os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()), relative)


def _profile(spec):
    ratios = spec.get("percent_triangles")
    screens = spec.get("screen_sizes")
    if not isinstance(ratios, list) or not isinstance(screens, list) or len(ratios) != 4 or len(screens) != 4:
        raise ValueError("each asset needs four explicit percent_triangles and screen_sizes")
    for name, values in (("percent_triangles", ratios), ("screen_sizes", screens)):
        if any(type(value) not in (int, float) or not math.isfinite(value) or value <= 0 or value > 1 for value in values):
            raise ValueError("%s must contain finite values in (0, 1]" % name)
        if abs(values[0] - 1.0) > 1e-6 or any(a <= b for a, b in zip(values, values[1:])):
            raise ValueError("%s must start at 1 and decrease strictly" % name)
    return [float(value) for value in ratios], [float(value) for value in screens]


def _vec(value):
    return [round(float(value.x), 6), round(float(value.y), 6), round(float(value.z), 6)]


def _lod0_geometry(mesh):
    description = mesh.get_static_mesh_description(0)
    if not description:
        raise RuntimeError("LOD0 source MeshDescription is unavailable")
    digest = hashlib.sha256()
    vertex_count = int(description.get_vertex_count())
    triangle_count = int(description.get_triangle_count())
    for index in range(vertex_count):
        vertex = unreal.VertexID(id_value=index)
        if not description.is_vertex_valid(vertex):
            raise RuntimeError("sparse LOD0 vertex IDs require an explicit inspection")
        position = description.get_vertex_position(vertex)
        digest.update(json.dumps([float(position.x), float(position.y), float(position.z)],
                                 separators=(",", ":")).encode("ascii"))
    for index in range(triangle_count):
        triangle = unreal.TriangleID(id_value=index)
        if not description.is_triangle_valid(triangle):
            raise RuntimeError("sparse LOD0 triangle IDs require an explicit inspection")
        vertices = [description.get_vertex_instance_vertex(description.get_triangle_vertex_instance(triangle, corner))
                    for corner in range(3)]
        digest.update(json.dumps([int(item.get_editor_property("id_value")) for item in vertices],
                                 separators=(",", ":")).encode("ascii"))
    return {"source_vertices": vertex_count, "source_triangles": triangle_count,
            "positions_topology_sha256": digest.hexdigest()}


def _preserved(mesh, editor):
    materials = []
    for item in mesh.get_editor_property("static_materials"):
        material = item.get_editor_property("material_interface")
        materials.append({
            "material": material.get_path_name() if material else None,
            "slot_name": str(item.get_editor_property("material_slot_name")),
            "imported_slot_name": str(item.get_editor_property("imported_material_slot_name")),
        })
    section_count = int(mesh.get_num_sections(0))
    section_materials = [int(editor.get_lod_material_slot(mesh, 0, index))
                         for index in range(section_count)]
    if any(index < 0 or index >= len(materials) for index in section_materials):
        raise RuntimeError("LOD0 section references an invalid material slot")
    body = mesh.get_editor_property("body_setup")
    collision = None
    if body:
        aggregate = body.get_editor_property("agg_geom")
        shape_counts = {name: len(aggregate.get_editor_property(name)) for name in (
            "sphere_elems", "box_elems", "sphyl_elems", "convex_elems",
            "tapered_capsule_elems", "level_set_elems", "skinned_level_set_elems")}
        simple_shapes = int(editor.get_simple_collision_count(mesh))
        # UE5.4 GetSimpleCollisionCount counts only spheres, boxes and capsules.
        if simple_shapes != sum(shape_counts[name] for name in (
                "sphere_elems", "box_elems", "sphyl_elems")):
            raise RuntimeError("BodySetup and editor simple-collision counts disagree")
        collision = {
            "object": body.get_path_name(),
            "trace_flag": str(body.get_editor_property("collision_trace_flag")),
            "double_sided": bool(body.get_editor_property("double_sided_geometry")),
            "aggregate_geometry": aggregate.export_text(),
            "shape_counts": shape_counts,
            "simple_shapes": simple_shapes,
        }
    sockets = []
    socket_probe = unreal.StaticMeshComponent()
    if not socket_probe.set_static_mesh(mesh):
        raise RuntimeError("transient socket probe could not reference StaticMesh")
    try:
        socket_names = list(socket_probe.get_all_socket_names())
    finally:
        socket_probe.set_static_mesh(None)
    if len(set(str(name) for name in socket_names)) != len(socket_names):
        raise RuntimeError("duplicate socket names prevent complete preservation checks")
    for socket_name in socket_names:
        socket = mesh.find_socket(socket_name)
        if not socket:
            raise RuntimeError("enumerated StaticMesh socket was not found")
        sockets.append({
            "name": str(socket.get_editor_property("socket_name")),
            "location": _vec(socket.get_editor_property("relative_location")),
            "rotation": socket.get_editor_property("relative_rotation").export_text(),
            "scale": _vec(socket.get_editor_property("relative_scale")),
        })
    return {
        "lod0_geometry": _lod0_geometry(mesh),
        "lod0_reduction_settings": editor.get_lod_reduction_settings(mesh, 0).export_text(),
        "lod0_triangles": int(mesh.get_num_triangles(0)),
        "lod0_vertices": int(editor.get_number_verts(mesh, 0)),
        "lod0_section_materials": section_materials,
        "material_slots": materials,
        "collision": collision,
        "sockets": sockets,
    }


def _snapshot(mesh, editor):
    count = int(mesh.get_num_lods())
    if count < 1:
        raise RuntimeError("StaticMesh has no render LOD")
    sizes = [float(size) for size in editor.get_lod_screen_sizes(mesh)]
    if len(sizes) != count:
        raise RuntimeError("LOD screen-size readback count differs from LOD count")
    bounds = mesh.get_bounds()
    return {
        "render_bounds": {"origin": _vec(bounds.origin), "extent": _vec(bounds.box_extent),
                          "sphere_radius": round(float(bounds.sphere_radius), 6)},
        "lod_count": count,
        "lod_triangles": [int(mesh.get_num_triangles(i)) for i in range(count)],
        "lod_vertices": [int(editor.get_number_verts(mesh, i)) for i in range(count)],
        "lod_section_materials": [[int(editor.get_lod_material_slot(mesh, i, section))
                                    for section in range(mesh.get_num_sections(i))] for i in range(count)],
        "screen_sizes": sizes,
        "auto_compute_screen_size": bool(mesh.is_lod_screen_size_auto_computed()),
        "preserved": _preserved(mesh, editor),
    }


def _validate_target(snapshot, screens):
    if snapshot["lod_count"] != 4:
        raise RuntimeError("expected 4 LODs, got %d" % snapshot["lod_count"])
    for name in ("lod_triangles", "lod_vertices"):
        values = snapshot[name]
        if any(value <= 0 for value in values) or any(a <= b for a, b in zip(values, values[1:])):
            raise RuntimeError("%s must be positive and strictly decreasing: %s" % (name, values))
    material_count = len(snapshot["preserved"]["material_slots"])
    for sections in snapshot["lod_section_materials"]:
        if not sections or any(index < 0 or index >= material_count for index in sections):
            raise RuntimeError("generated LOD has an invalid render-section material mapping")
    if snapshot["auto_compute_screen_size"]:
        raise RuntimeError("LOD screen sizes are still automatic")
    if any(abs(actual - expected) > 1e-5 for actual, expected in zip(snapshot["screen_sizes"], screens)):
        raise RuntimeError("LOD screen-size readback differs: %s" % snapshot["screen_sizes"])


def _backup(source, destination):
    if not os.path.isfile(source):
        raise RuntimeError("source uasset missing: %s" % source)
    original_hash = _sha256(source)
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    if os.path.exists(destination):
        if _sha256(destination) != original_hash:
            raise RuntimeError("existing backup differs from current source: %s" % destination)
    else:
        shutil.copy2(source, destination)
    if _sha256(destination) != original_hash:
        raise RuntimeError("uasset backup hash mismatch: %s" % destination)
    return original_hash


def _write_report(path, report):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as output:
        json.dump(report, output, ensure_ascii=False, indent=2)
    os.replace(temporary, path)


def run(manifest_path, mode="inspect"):
    if mode not in ("inspect", "apply", "verify"):
        raise ValueError("mode must be inspect, apply, or verify")
    manifest_path = _saved_path(manifest_path, "manifest_path")
    with open(manifest_path, encoding="utf-8") as source:
        manifest = json.load(source)
    report_path = _saved_path(manifest.get("report_path"), "report_path")
    if report_path == manifest_path:
        raise ValueError("report_path must differ from manifest_path")
    backup_dir = _saved_path(manifest.get("backup_dir"), "backup_dir") if mode == "apply" else None
    specs = manifest.get("assets")
    report = {"mode": mode, "manifest_path": manifest_path, "assets": [], "errors": []}
    previous = None
    try:
        if mode == "verify":
            if not os.path.isfile(report_path):
                raise RuntimeError("verify requires a completed apply report")
            with open(report_path, encoding="utf-8") as source:
                previous = json.load(source)
            if previous.get("errors") or previous.get("mode") not in ("apply", "verify"):
                raise RuntimeError("verify requires a successful apply or verify report")
        if not isinstance(specs, list) or not specs:
            raise ValueError("manifest.assets must be a nonempty list")
        paths = [_asset_path(spec.get("path")) for spec in specs]
        if len(set(paths)) != len(paths):
            raise ValueError("manifest contains duplicate StaticMesh paths")
        profiles = [_profile(spec) for spec in specs]
        if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
            raise RuntimeError("PIE is active; StaticMeshEditorSubsystem LOD access refused")
        editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        for spec, path, (ratios, screens) in zip(specs, paths, profiles):
            row = {"path": path, "percent_triangles": ratios, "expected_screen_sizes": screens}
            report["assets"].append(row)
            try:
                mesh = unreal.EditorAssetLibrary.load_asset(path)
                if not mesh or mesh.get_class().get_name() != "StaticMesh":
                    raise RuntimeError("asset missing or not StaticMesh: %s" % path)
                before = _snapshot(mesh, editor)
                row["before"] = before
                if mode == "inspect":
                    row["status"] = "needs_lods" if before["lod_count"] == 1 else "existing_lods"
                    continue
                dirty = {package.get_name() for package in
                         unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
                if path in dirty:
                    raise RuntimeError("source package has unsaved editor changes; save or discard before LOD validation")
                if before["lod_count"] == 4:
                    _validate_target(before, screens)
                    if mode == "verify" and previous:
                        prior_rows = {item["path"]: item for item in previous.get("assets", [])}
                        prior = prior_rows.get(path)
                        baseline = prior.get("apply_baseline") if prior else None
                        if prior and previous.get("mode") == "apply" and prior.get("status") == "authored_and_saved":
                            baseline = {"preserved": prior["before"]["preserved"],
                                        "saved_uasset_sha256": prior["saved_uasset_sha256"]}
                        if not baseline:
                            raise RuntimeError("apply preservation baseline is missing")
                        if baseline:
                            if before["preserved"] != baseline["preserved"]:
                                raise RuntimeError("saved LOD0 geometry/material/collision/socket drifted")
                            actual_hash = _sha256(_source_file(path))
                            if actual_hash != baseline["saved_uasset_sha256"]:
                                raise RuntimeError("saved uasset differs from apply report")
                            row["saved_uasset_sha256"] = actual_hash
                            row["apply_report_matched"] = True
                            row["apply_baseline"] = baseline
                    if mode == "apply":
                        row["saved_uasset_sha256"] = _sha256(_source_file(path))
                        row["apply_baseline"] = {"preserved": before["preserved"],
                                                 "saved_uasset_sha256": row["saved_uasset_sha256"]}
                    row["status"] = "already_target"
                    row["after"] = before
                    continue
                if mode == "verify":
                    raise RuntimeError("target LODs are absent; found %d" % before["lod_count"])
                if before["lod_count"] != 1:
                    raise RuntimeError("refusing existing %d-LOD StaticMesh" % before["lod_count"])
                if abs(float(editor.get_lod_reduction_settings(mesh, 0).percent_triangles) - ratios[0]) > 1e-6:
                    raise RuntimeError("LOD0 already has a reduction; refusing to change its settings")
                source_file = _source_file(path)
                relative = os.path.relpath(source_file, unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()))
                backup_file = os.path.join(backup_dir, relative)
                original_hash = _backup(source_file, backup_file)
                row["backup"] = {"file": backup_file, "sha256": original_hash}
                options = unreal.StaticMeshReductionOptions()
                options.set_editor_property("auto_compute_lod_screen_size", False)
                settings = []
                for ratio, screen in zip(ratios, screens):
                    item = unreal.StaticMeshReductionSettings()
                    item.set_editor_property("percent_triangles", ratio)
                    item.set_editor_property("screen_size", screen)
                    settings.append(item)
                options.set_editor_property("reduction_settings", settings)
                count = int(editor.set_lods(mesh, options))
                if count != 4:
                    raise RuntimeError("SetLods returned %d, expected 4" % count)
                if not editor.set_lod_screen_sizes(mesh, screens):
                    raise RuntimeError("SetLodScreenSizes returned false")
                after = _snapshot(mesh, editor)
                row["after"] = after
                _validate_target(after, screens)
                if after["preserved"] != before["preserved"]:
                    raise RuntimeError("LOD0 geometry/material/collision/socket preservation failed")
                if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
                    raise RuntimeError("save_loaded_asset returned false")
                saved_snapshot = _snapshot(mesh, editor)
                _validate_target(saved_snapshot, screens)
                if saved_snapshot["preserved"] != before["preserved"]:
                    raise RuntimeError("preserved mesh data changed during save")
                row["after_save"] = saved_snapshot
                if not os.path.isfile(source_file):
                    raise RuntimeError("saved uasset missing")
                saved_hash = _sha256(source_file)
                if saved_hash == original_hash:
                    raise RuntimeError("saved uasset hash unchanged after LOD authoring")
                row["saved_uasset_sha256"] = saved_hash
                row["status"] = "authored_and_saved"
            except Exception as error:
                row["error"] = "%s: %s" % (type(error).__name__, error)
                report["errors"].append({"path": path, "error": row["error"]})
                raise
    except Exception as error:
        if not report["errors"]:
            report["errors"].append({"error": "%s: %s" % (type(error).__name__, error)})
        report["traceback"] = traceback.format_exc()
        _write_report(report_path, report)
        raise
    _write_report(report_path, report)
    return report
