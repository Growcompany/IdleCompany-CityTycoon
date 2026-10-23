"""Partition one source StaticMesh LOD0 into render-only XY grid chunks.

Run prepare/verify from the UE5.4 editor Python console. Neither function edits
the source mesh or a map. prepare updates planned existing chunks only when
update_existing=True; the caller backs up those assets before that operation.
"""

from collections import Counter, defaultdict
import hashlib
import json
import math
import os
import re
import traceback

import unreal


SOURCE = "/Game/City_Pack/Meshes/Terrain/SM_IslandTerrainMesh"
QUERY = unreal.GeometryScript_MeshQueries
LIST = unreal.GeometryScript_List
MATERIALS = unreal.GeometryScript_Materials
ASSETS = unreal.GeometryScript_AssetUtils
SELECTION = unreal.GeometryScript_MeshSelection
DECOMPOSITION = unreal.GeometryScript_MeshDecomposition
NEW_ASSETS = unreal.GeometryScript_NewAssetUtils


def _report_path(path):
    if not isinstance(path, str) or not os.path.isabs(path):
        raise ValueError("report_path must be absolute under project Saved")
    saved_roots = {os.path.normcase(os.path.realpath(
        unreal.Paths.convert_relative_path_to_full(saved))) for saved in (
        unreal.Paths.project_saved_dir(), os.path.join(unreal.Paths.project_dir(), 'Saved'))}
    path = os.path.normcase(os.path.realpath(path))
    if not any(os.path.commonpath((saved, path)) == saved for saved in saved_roots):
        raise ValueError("report_path must be under project Saved")
    return path


def _package(path):
    if not isinstance(path, str) or not re.fullmatch(r"/Game/[A-Za-z0-9_/-]+", path):
        raise ValueError("expected exact /Game package path: %r" % path)
    return path


def _source_file(path):
    content = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir())
    return os.path.join(content, path[len("/Game/"):].replace("/", os.sep) + ".uasset")


def _sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _write_report(path, report):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as stream:
        json.dump(report, stream, ensure_ascii=False, indent=2)
    os.replace(temporary, path)


def _ready_source(path):
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("PIE must be stopped")
    dirty = {package.get_name() for package in
             unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    if path in dirty:
        raise RuntimeError("source mesh package has unsaved changes")
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh or mesh.get_class().get_name() != "StaticMesh":
        raise RuntimeError("source is not a StaticMesh: " + path)
    if mesh.get_num_lods() != 1:
        raise RuntimeError("expected a one-LOD source mesh")
    return mesh


def _read(mesh):
    lod = unreal.GeometryScriptMeshReadLOD()
    lod.set_editor_property("lod_type", unreal.GeometryScriptLODType.SOURCE_MODEL)
    lod.set_editor_property("lod_index", 0)
    options = unreal.GeometryScriptCopyMeshFromAssetOptions()
    # Compute the source shading basis before splitting so borders share it.
    options.set_editor_property("apply_build_settings", True)
    options.set_editor_property("request_tangents", True)
    options.set_editor_property("use_build_scale", False)
    dynamic, outcome = ASSETS.copy_mesh_from_static_mesh(mesh, unreal.DynamicMesh(), options, lod)
    if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
        raise RuntimeError("cannot read source-model LOD0: " + mesh.get_path_name())
    return dynamic


def _numbers(value, names, digits):
    return tuple(round(float(getattr(value, name)), digits) for name in names)


def _triangle(dynamic, triangle_id, channels):
    valid, a, b, c = QUERY.get_triangle_positions(dynamic, triangle_id)
    if not valid:
        raise RuntimeError("invalid triangle ID: %d" % triangle_id)
    positions = [_numbers(v, ("x", "y", "z"), 3) for v in (a, b, c)]
    uvs = [[] for _ in range(3)]
    for channel in range(channels):
        u, v, w, valid_uv = QUERY.get_triangle_u_vs(dynamic, channel, triangle_id)
        if not valid_uv:
            raise RuntimeError("missing UV channel %d in triangle %d" % (channel, triangle_id))
        for corner, uv in enumerate((u, v, w)):
            uvs[corner].append(_numbers(uv, ("x", "y"), 6))
    _, has_normals, normals, tangents, bitangents = QUERY.get_triangle_normal_tangents(
        dynamic, triangle_id)
    frames = []
    for corner in range(3):
        frames.append(tuple(_numbers(v.get_editor_property("vector" + str(corner)),
                                      ("x", "y", "z"), 5)
                            for v in (normals, tangents, bitangents)) if has_normals else ())
    _, ca, cb, cc, has_colors = QUERY.get_triangle_vertex_colors(dynamic, triangle_id)
    colors = [_numbers(v, ("r", "g", "b", "a"), 6) for v in (ca, cb, cc)] if has_colors else [()] * 3
    material_id, valid_material = MATERIALS.get_triangle_material_id(dynamic, triangle_id)
    if not valid_material:
        raise RuntimeError("missing material ID in triangle %d" % triangle_id)
    corners = tuple((positions[i], tuple(uvs[i]), frames[i], colors[i]) for i in range(3))
    # A cyclic rotation preserves winding while ignoring the arbitrary first corner.
    canonical = min(corners[i:] + corners[:i] for i in range(3))
    return (int(material_id), bool(has_normals), bool(has_colors), canonical), (
        (a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0)


def _records(dynamic):
    _, indices, _ = QUERY.get_all_triangle_i_ds(dynamic)
    triangle_ids = [int(i) for i in LIST.convert_index_list_to_array(indices)]
    channels = int(QUERY.get_num_uv_sets(dynamic))
    return {tid: _triangle(dynamic, tid, channels) for tid in triangle_ids}, channels


def _digest(signatures):
    digest = hashlib.sha256()
    for signature, count in sorted(Counter(signatures).items()):
        digest.update(json.dumps([signature, count], separators=(",", ":")).encode("ascii"))
    return digest.hexdigest()


def _require_material_slots(mesh, source_materials):
    actual_materials = list(mesh.get_editor_property("static_materials"))
    if len(actual_materials) != len(source_materials):
        raise RuntimeError("material slot count changed")
    for original, actual in zip(source_materials, actual_materials):
        for property_name in ("material_interface", "material_slot_name", "imported_material_slot_name"):
            if original.get_editor_property(property_name) != actual.get_editor_property(property_name):
                raise RuntimeError("material slot drift: " + property_name)


def _validate_chunk(mesh, expected, source_materials, editor):
    if mesh.get_num_lods() != 1:
        raise RuntimeError("chunk has unexpected LOD count")
    _require_material_slots(mesh, source_materials)
    section_slots = [int(editor.get_lod_material_slot(mesh, 0, section))
                     for section in range(mesh.get_num_sections(0))]
    if not section_slots or any(slot < 0 or slot >= len(source_materials) for slot in section_slots):
        raise RuntimeError("invalid render section material mapping")
    actual, _ = _records(_read(mesh))
    if Counter(signature for signature, _ in actual.values()) != Counter(expected):
        raise RuntimeError("chunk LOD0 geometry or corner attributes differ from source selection")
    return {"triangles": len(actual), "signature_sha256": _digest(expected),
            "render_section_slots": section_slots}


def prepare(source_path, destination_folder, report_path, cell_size_cm=20000,
            update_existing=False):
    """Create planned chunks, or update their LOD0 after the caller backs them up.

    Existing assets outside the current plan and all map actors are untouched.
    """
    source_path = _package(source_path)
    destination_folder = _package(destination_folder).rstrip("/")
    report_path = _report_path(report_path)
    if source_path != SOURCE:
        raise ValueError("this authoring tool is scoped to " + SOURCE)
    if type(cell_size_cm) not in (int, float) or not math.isfinite(cell_size_cm) or cell_size_cm <= 0:
        raise ValueError("cell_size_cm must be finite and positive")
    if type(update_existing) is not bool:
        raise ValueError("update_existing must be a bool")
    report = {"status": "started", "source": source_path, "destination_folder": destination_folder,
              "cell_size_cm": float(cell_size_cm), "apply_source_build_settings": True,
              "update_existing": update_existing, "chunks": [], "errors": []}
    try:
        source = _ready_source(source_path)
        source_file = _source_file(source_path)
        report["source_sha256"] = _sha256(source_file)
        source_materials = list(source.get_editor_property("static_materials"))
        if len(source_materials) != 1:
            raise RuntimeError("expected exactly one source material slot")
        editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        source_sections = [int(editor.get_lod_material_slot(source, 0, section))
                           for section in range(source.get_num_sections(0))]
        if not source_sections or any(slot != 0 for slot in source_sections):
            raise RuntimeError("expected source render sections to use material slot 0")
        source_dynamic = _read(source)
        records, channels = _records(source_dynamic)
        if not records or len(records) != int(source.get_static_mesh_description(0).get_triangle_count()):
            raise RuntimeError("source triangle count does not match source-model LOD0")
        if any(signature[0] != 0 for signature, _ in records.values()):
            raise RuntimeError("expected source triangles to use material ID 0")
        report["source_triangles"] = len(records)
        report["source_uv_channels"] = channels
        report["source_signature_sha256"] = _digest(signature for signature, _ in records.values())
        cells = defaultdict(list)
        for triangle_id, (_, (x, y)) in records.items():
            cells[(math.floor(x / cell_size_cm), math.floor(y / cell_size_cm))].append(triangle_id)
        planned = {(x, y): "%s/SM_IslandChunk_%s_%s" % (destination_folder, x, y)
                   for x, y in cells}
        collisions = [path for path in planned.values() if unreal.EditorAssetLibrary.does_asset_exist(path)]
        if collisions and not update_existing:
            raise RuntimeError("destination assets already exist: " + ", ".join(sorted(collisions)))
        existing = {}
        if update_existing:
            dirty = {package.get_name() for package in
                     unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
            for path in sorted(collisions):
                if path in dirty:
                    raise RuntimeError("destination asset has unsaved changes: " + path)
                asset = unreal.EditorAssetLibrary.load_asset(path)
                if not asset or asset.get_class().get_name() != "StaticMesh":
                    raise RuntimeError("destination is not a StaticMesh: " + path)
                if asset.get_num_lods() != 1:
                    raise RuntimeError("destination must have one LOD: " + path)
                _require_material_slots(asset, source_materials)
                existing[path] = asset
        report["planned_chunks"] = len(planned)
        report["existing_chunks"] = len(existing)
        options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        for name in ("enable_recompute_normals", "enable_recompute_tangents", "enable_nanite",
                     "enable_collision"):
            options.set_editor_property(name, False)
        write_options = None
        target_lod = None
        if existing:
            write_options = unreal.GeometryScriptCopyMeshToAssetOptions()
            for name in ("enable_recompute_normals", "enable_recompute_tangents",
                         "enable_remove_degenerates", "use_build_scale", "replace_materials",
                         "apply_nanite_settings", "emit_transaction", "defer_mesh_post_edit_change"):
                write_options.set_editor_property(name, False)
            target_lod = unreal.GeometryScriptMeshWriteLOD()
            target_lod.set_editor_property("lod_index", 0)
            target_lod.set_editor_property("write_hi_res_source", False)
        for cell, path in sorted(planned.items()):
            ids = sorted(cells[cell])
            row = {"path": path, "cell": list(cell), "source_triangle_ids": ids,
                   "source_id_range": [ids[0], ids[-1]], "source_triangles": len(ids),
                   "action": "update" if path in existing else "create",
                   "status": "updating" if path in existing else "creating"}
            report["chunks"].append(row)
            _, selection = SELECTION.convert_index_array_to_mesh_selection(
                source_dynamic, ids, unreal.GeometryScriptMeshSelectionType.TRIANGLES)
            chunk_dynamic = unreal.DynamicMesh()
            DECOMPOSITION.copy_mesh_selection_to_mesh(
                source_dynamic, chunk_dynamic, selection, False, True)
            if path in existing:
                asset = existing[path]
                _, outcome = ASSETS.copy_mesh_to_static_mesh(
                    chunk_dynamic, asset, write_options, target_lod)
                if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
                    raise RuntimeError("chunk LOD0 update failed: " + path)
                row["status"] = "updated"
            else:
                asset, outcome = NEW_ASSETS.create_new_static_mesh_asset_from_mesh(
                    chunk_dynamic, path, options)
                if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or not asset:
                    raise RuntimeError("chunk asset creation failed: " + path)
                row["status"] = "created"
                asset.set_editor_property("static_materials", source_materials)
            expected = [records[tid][0] for tid in ids]
            row.update(_validate_chunk(asset, expected, source_materials, editor))
            if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("chunk save failed: " + path)
            row["saved_sha256"] = _sha256(_source_file(path))
            row["status"] = "saved_and_validated"
        if _sha256(source_file) != report["source_sha256"]:
            raise RuntimeError("source uasset changed during authoring")
        report["status"] = "prepared"
    except Exception as error:
        report["status"] = "failed"
        report["errors"].append(str(error))
        report["traceback"] = traceback.format_exc()
        raise
    finally:
        _write_report(report_path, report)
    return report


def verify(source_path, report_path):
    source_path = _package(source_path)
    report_path = _report_path(report_path)
    with open(report_path, encoding="utf-8") as stream:
        report = json.load(stream)
    if source_path != SOURCE or report.get("source") != source_path or report.get("status") != "prepared":
        raise RuntimeError("verify requires a prepared report for the island terrain source")
    source = _ready_source(source_path)
    if _sha256(_source_file(source_path)) != report["source_sha256"]:
        raise RuntimeError("source uasset differs from prepared report")
    records, _ = _records(_read(source))
    if len(records) != report["source_triangles"]:
        raise RuntimeError("source triangle count changed")
    covered = []
    source_materials = list(source.get_editor_property("static_materials"))
    editor = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    for row in report["chunks"]:
        path = _package(row["path"])
        mesh = unreal.EditorAssetLibrary.load_asset(path)
        if not mesh or _sha256(_source_file(path)) != row["saved_sha256"]:
            raise RuntimeError("missing or changed saved chunk: " + path)
        ids = row["source_triangle_ids"]
        expected = [records[tid][0] for tid in ids]
        result = _validate_chunk(mesh, expected, source_materials, editor)
        if result["signature_sha256"] != row["signature_sha256"]:
            raise RuntimeError("chunk signature differs from report: " + path)
        covered.extend(ids)
    if len(covered) != len(records) or len(set(covered)) != len(records) or set(covered) != set(records):
        raise RuntimeError("chunk membership does not cover source triangles exactly once")
    return {"status": "verified", "source_triangles": len(records),
            "chunk_count": len(report["chunks"])}
