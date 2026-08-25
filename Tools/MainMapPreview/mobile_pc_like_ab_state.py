"""Pure state/diagnostic helpers for the MainMap mobile visual capture."""

import hashlib
import json
import math
import os
import re
import uuid

from mobile_cinematic_filter import (
    BASELINE_SAFETY_TIMES,
    CF1_TIMES,
    cf1_saved_dir_suffix,
)


_CF1_STAGES = ("BASELINE_SAFETY", "AB")
_CF1_VARIANTS = ("A_PRE", "B", "A_POST")
_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
_CF1_RESULT_RELATIVE_ROOT = os.path.join(
    "Saved",
    "BuildingLookdev",
    "MainMapPreview",
    "CinematicFilterStrongAB",
    "CF1",
)
_EXPECTED_MAINMAP_PATH = (
    "/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity."
    "MainMap_TheRiverwalkCity"
)
_TRANSFORM_KEYS = {"location", "rotation", "scale"}
_SCENE_KEYS = {
    "schema_version",
    "map_object_path",
    "city_buildings",
    "plots",
    "placed_buildings",
}
_CITY_BUILDING_KEYS = {"city_key", "class_object_path", "transform"}
_PLOT_KEYS = {"plot_id", "owned", "transform"}
_PLACED_BUILDING_KEYS = {
    "building_index",
    "building_id",
    "plot_id",
    "company_type",
    "body_module_copies",
    "floor_height_body_module_scale",
    "uv_layout_selection",
    "walls_between_windows_switch",
    "applied_skin_id",
    "applied_light_id",
    "transform",
}


def canonical_cf1_stage(stage):
    if stage not in _CF1_STAGES:
        raise ValueError("CF1 stage must be BASELINE_SAFETY or AB")
    return stage


def canonical_cf1_run_id(run_id):
    if not isinstance(run_id, str):
        raise ValueError("CF1 run ID must be a canonical lowercase UUID")
    try:
        parsed = uuid.UUID(run_id)
    except (ValueError, AttributeError) as exc:
        raise ValueError(
            "CF1 run ID must be a canonical lowercase UUID"
        ) from exc
    if str(parsed) != run_id:
        raise ValueError("CF1 run ID must be a canonical lowercase UUID")
    return run_id


def experiment_phase_plan(experiment, stage):
    if experiment != "CF1":
        raise ValueError("stage-aware phase plans are defined only for CF1")
    canonical_cf1_stage(stage)
    times = (
        tuple(BASELINE_SAFETY_TIMES)
        if stage == "BASELINE_SAFETY"
        else tuple(CF1_TIMES)
    )
    variants = ("A_PRE",) if stage == "BASELINE_SAFETY" else _CF1_VARIANTS
    return tuple(
        (variant, time_code)
        for time_code in times
        for variant in variants
    )


def cf1_run_paths(stage, run_id, project_root):
    canonical_cf1_stage(stage)
    canonical_cf1_run_id(run_id)
    if not isinstance(project_root, (str, os.PathLike)):
        raise ValueError("project_root must be a path")
    suffix = cf1_saved_dir_suffix(stage, run_id)
    relative_output_dir = os.path.join(
        _CF1_RESULT_RELATIVE_ROOT,
        stage,
        run_id,
        "VulkanES31",
    )
    output_dir = os.path.normpath(
        os.path.join(os.fspath(project_root), relative_output_dir)
    )
    sequence = experiment_phase_plan("CF1", stage)
    return {
        "stage": stage,
        "run_id": run_id,
        "saved_dir_suffix": suffix,
        "expected_project_saved_dir": os.path.normpath(
            os.path.join(os.fspath(project_root), "Saved_" + suffix)
        ),
        "relative_output_dir": relative_output_dir.replace("\\", "/"),
        "output_dir": output_dir,
        "marker": os.path.join(output_dir, "result_marker.json"),
        "log": os.path.join(output_dir, "capture.log"),
        "images": tuple(
            os.path.join(
                output_dir,
                "MainMap_CF1_%s_%s.png" % (variant, time_code),
            )
            for variant, time_code in sequence
        ),
    }


def cf1_evidence_collision_errors(paths):
    if not isinstance(paths, dict):
        return ["CF1 run paths must be an object"]
    candidates = [paths.get("marker")]
    images = paths.get("images", ())
    if isinstance(images, (list, tuple)):
        candidates.extend(images)
    else:
        return ["CF1 image paths must be a sequence"]
    return [
        path
        for path in candidates
        if isinstance(path, str) and os.path.lexists(path)
    ]


def _flag_values(command_line, name):
    if not isinstance(command_line, str):
        raise ValueError("command line must be text")
    pattern = r"(?:^|\s)-" + re.escape(name) + r"=([^\s]*)"
    return re.findall(pattern, command_line, flags=re.IGNORECASE)


def _one_flag(command_line, name):
    values = _flag_values(command_line, name)
    if len(values) != 1 or values[0] == "":
        raise ValueError("exactly one -%s value is required" % name)
    return values[0]


def cf1_launch_contract_from_command_line(command_line):
    stage = canonical_cf1_stage(_one_flag(command_line, "CGRStage"))
    run_id = canonical_cf1_run_id(_one_flag(command_line, "CGRRunId"))
    seed_sha = _one_flag(command_line, "CGRSeedSlotSha256")
    if not _SHA256_PATTERN.fullmatch(seed_sha):
        raise ValueError("CGRSeedSlotSha256 must be lowercase SHA-256")
    if _one_flag(command_line, "CGRProbe") != "MobilePCLikeAB":
        raise ValueError("CGRProbe must be MobilePCLikeAB")
    if _one_flag(command_line, "CGRExperiment") != "CF1":
        raise ValueError("CGRExperiment must be CF1")
    if _one_flag(command_line, "CGRAdopted") != "E2,L2":
        raise ValueError("CGRAdopted must be E2,L2")
    expected_suffix = cf1_saved_dir_suffix(stage, run_id)
    suffix = _one_flag(command_line, "saveddirsuffix")
    if suffix != expected_suffix:
        raise ValueError("saveddirsuffix does not match the CF1 run")
    tokens = command_line.lower().split()
    if tokens.count("-vulkan") != 1:
        raise ValueError("exactly one Vulkan launch flag is required")
    if tokens.count("-featureleveles31") != 1:
        raise ValueError("exactly one ES3.1 launch flag is required")
    conflicting_rhi = {
        "-d3d11", "-d3d12", "-opengl", "-opengl4", "-metal", "-nullrhi"
    }
    if any(token in conflicting_rhi for token in tokens):
        raise ValueError("conflicting RHI launch flags are forbidden")
    if any(
        token.startswith("-featurelevel")
        and token != "-featureleveles31"
        for token in tokens
    ):
        raise ValueError("conflicting feature-level flags are forbidden")
    return {
        "stage": stage,
        "run_id": run_id,
        "seed_slot_sha256": seed_sha,
        "saved_dir_suffix": suffix,
        "sequence": experiment_phase_plan("CF1", stage),
    }


def runner_launch_contract_from_command_line(command_line, legacy_parser):
    experiment_values = _flag_values(command_line, "CGRExperiment")
    if experiment_values == ["CF1"]:
        strict = cf1_launch_contract_from_command_line(command_line)
        return "CF1", ("E2", "L2"), strict
    experiment, adopted = legacy_parser(command_line)
    return experiment, adopted, None


def cf1_runtime_state_errors(runtime_state):
    if not isinstance(runtime_state, dict):
        return ["runtime_state must be an object"]
    errors = []

    def require_equal(field, expected):
        actual = runtime_state.get(field)
        if actual != expected:
            errors.append("%s: expected %r, got %r" % (field, expected, actual))

    actual_saved = runtime_state.get("project_saved_dir")
    expected_saved = runtime_state.get("expected_project_saved_dir")
    if not isinstance(actual_saved, str) or not isinstance(expected_saved, str):
        errors.append("project_saved_dir: both paths must be text")
    elif os.path.normpath(actual_saved) != os.path.normpath(expected_saved):
        errors.append(
            "project_saved_dir: expected %r, got %r"
            % (expected_saved, actual_saved)
        )
    seed_sha = runtime_state.get("seed_slot_sha256")
    if not isinstance(seed_sha, str) or not _SHA256_PATTERN.fullmatch(seed_sha):
        errors.append("seed_slot_sha256: expected lowercase SHA-256")
    for field in ("isolated_slot_sha256", "root_slot_sha256"):
        require_equal(field, seed_sha)
    require_equal("wipe_save_on_launch", False)
    require_equal("start_mode", 0)
    require_equal("save_exists", True)
    require_equal("load_succeeded", True)
    require_equal("experiment", "CF1")
    require_equal("adopted", ["E2", "L2"])
    require_equal("renderer", "Vulkan")
    require_equal("feature_level", "ES3_1")
    suffix = runtime_state.get("saved_dir_suffix")
    if not isinstance(expected_saved, str) or not isinstance(suffix, str):
        errors.append("saved_dir_suffix: expected a derived suffix")
    elif os.path.basename(os.path.normpath(expected_saved)) != "Saved_" + suffix:
        errors.append("saved_dir_suffix: does not match project_saved_dir")
    return errors


def capture_file_identity(path):
    if not os.path.isfile(path):
        return None
    stat_result = os.stat(path)
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return {
        "path": os.path.normpath(path),
        "device": int(stat_result.st_dev),
        "inode": int(stat_result.st_ino),
        "mtime_ns": int(stat_result.st_mtime_ns),
        "size": int(stat_result.st_size),
        "sha256": digest.hexdigest(),
    }


def cf1_evidence_fingerprints(paths):
    if not isinstance(paths, dict):
        raise ValueError("CF1 run paths must be an object")
    fingerprints = {}
    for label in ("marker", "log"):
        path = paths.get(label)
        if not isinstance(path, str):
            raise ValueError("CF1 %s path must be text" % label)
        identity = capture_file_identity(path)
        if identity is None:
            raise ValueError("CF1 %s evidence is missing" % label)
        fingerprints[label] = {
            "path": identity["path"],
            "size": identity["size"],
            "sha256": identity["sha256"],
        }
    return fingerprints


def cf1_capture_evidence_recheck_errors(sequence, evidence):
    if not isinstance(sequence, (list, tuple)) or not isinstance(evidence, dict):
        return ["CF1 capture sequence/evidence is malformed"]
    errors = []
    for variant, time_code in sequence:
        label = "%s_%s" % (variant, time_code)
        accepted = evidence.get(label)
        if not isinstance(accepted, dict):
            errors.append(label + " accepted evidence is missing")
            continue
        path = accepted.get("path")
        if not isinstance(path, str):
            errors.append(label + " accepted evidence path is malformed")
            continue
        current = capture_file_identity(path)
        if current is None:
            errors.append(label + " capture is missing at final recheck")
            continue
        expected = {
            "path": os.path.normpath(path),
            "mtime_ns": accepted.get("final_mtime_ns"),
            "size": accepted.get("size"),
            "sha256": accepted.get("sha256"),
        }
        actual = {
            field: current.get(field)
            for field in ("path", "mtime_ns", "size", "sha256")
        }
        if actual != expected:
            errors.append(label + " capture changed after acceptance")
    return errors


def screenshot_observation_errors(
    initial_identity,
    first_identity,
    second_identity,
    request_time_ns,
):
    errors = []
    if initial_identity is not None:
        errors.append("screenshot destination was not initially absent")
    for label, identity in (
        ("first", first_identity),
        ("second", second_identity),
    ):
        if not isinstance(identity, dict):
            errors.append("%s screenshot identity is missing" % label)
            continue
        if identity.get("size", 0) <= 0:
            errors.append("%s screenshot size is zero" % label)
        if identity.get("mtime_ns", -1) < request_time_ns:
            errors.append("%s screenshot identity predates request" % label)
    if isinstance(first_identity, dict) and isinstance(second_identity, dict):
        identity_fields = ("path", "device", "inode")
        if any(
            first_identity.get(field) != second_identity.get(field)
            for field in identity_fields
        ):
            errors.append("screenshot post-request identity changed")
        if first_identity.get("size") != second_identity.get("size"):
            errors.append("screenshot did not reach a stable size")
        if first_identity.get("sha256") != second_identity.get("sha256"):
            errors.append("screenshot content changed between stable callbacks")
    return errors


def _finite_number(value, field):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(field + " must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(field + " must be finite")
    return 0.0 if result == 0.0 else result


def _canonical_transform(value, field):
    if not isinstance(value, dict) or set(value) != _TRANSFORM_KEYS:
        raise ValueError(field + " must contain location/rotation/scale")
    result = {}
    for name in ("location", "rotation", "scale"):
        channels = value[name]
        if not isinstance(channels, (list, tuple)) or len(channels) != 3:
            raise ValueError(field + "." + name + " must have three values")
        normalized = [
            _finite_number(item, field + "." + name)
            for item in channels
        ]
        if name == "rotation":
            normalized = [((item + 180.0) % 360.0) - 180.0 for item in normalized]
            normalized = [0.0 if item == 0.0 else item for item in normalized]
        result[name] = normalized
    return result


def _exact_object(value, keys, field):
    if not isinstance(value, dict):
        raise ValueError(field + " must be an object")
    unknown = set(value) - keys
    missing = keys - set(value)
    if unknown or missing:
        raise ValueError(
            "%s keys differ (missing=%r unknown=%r)"
            % (field, sorted(missing), sorted(unknown))
        )


def _reject_duplicate_identity(items, key, field):
    seen = []
    for item in items:
        value = item[key]
        if value in seen:
            raise ValueError("%s contains duplicate %s" % (field, key))
        seen.append(value)


def _canonical_scene_object(scene):
    _exact_object(scene, _SCENE_KEYS, "loaded_scene")
    if scene["schema_version"] != 1:
        raise ValueError("loaded_scene schema_version must be 1")
    if not isinstance(scene["map_object_path"], str):
        raise ValueError("map_object_path must be text")
    result = {
        "schema_version": 1,
        "map_object_path": scene["map_object_path"],
        "city_buildings": [],
        "plots": [],
        "placed_buildings": [],
    }
    for index, item in enumerate(scene["city_buildings"]):
        field = "city_buildings[%d]" % index
        _exact_object(item, _CITY_BUILDING_KEYS, field)
        if isinstance(item["city_key"], bool) or not isinstance(item["city_key"], int):
            raise ValueError(field + ".city_key must be an integer")
        if not isinstance(item["class_object_path"], str):
            raise ValueError(field + ".class_object_path must be text")
        result["city_buildings"].append({
            "city_key": item["city_key"],
            "class_object_path": item["class_object_path"],
            "transform": _canonical_transform(item["transform"], field + ".transform"),
        })
    for index, item in enumerate(scene["plots"]):
        field = "plots[%d]" % index
        _exact_object(item, _PLOT_KEYS, field)
        if not isinstance(item["plot_id"], str) or not isinstance(item["owned"], bool):
            raise ValueError(field + " plot_id/owned types are invalid")
        result["plots"].append({
            "plot_id": item["plot_id"],
            "owned": item["owned"],
            "transform": _canonical_transform(item["transform"], field + ".transform"),
        })
    for index, item in enumerate(scene["placed_buildings"]):
        field = "placed_buildings[%d]" % index
        _exact_object(item, _PLACED_BUILDING_KEYS, field)
        copied = {
            key: item[key]
            for key in _PLACED_BUILDING_KEYS
            if key != "transform"
        }
        copied["transform"] = _canonical_transform(
            item["transform"], field + ".transform"
        )
        result["placed_buildings"].append(copied)
    _reject_duplicate_identity(
        result["city_buildings"], "city_key", "city_buildings"
    )
    _reject_duplicate_identity(result["plots"], "plot_id", "plots")
    _reject_duplicate_identity(
        result["placed_buildings"],
        "building_index",
        "placed_buildings",
    )
    result["city_buildings"].sort(key=lambda item: item["city_key"])
    result["plots"].sort(key=lambda item: item["plot_id"])
    result["placed_buildings"].sort(key=lambda item: item["building_index"])
    return result


def canonical_loaded_scene(scene):
    normalized = _canonical_scene_object(scene)
    return json.dumps(
        normalized,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def loaded_scene_sha256(scene):
    return hashlib.sha256(canonical_loaded_scene(scene)).hexdigest()


def cf1_load_gate_errors(slot_projection, runtime_scene):
    errors = []
    try:
        expected = _canonical_scene_object(slot_projection)
    except (TypeError, ValueError) as exc:
        return ["slot projection: %s" % exc]
    try:
        actual = _canonical_scene_object(runtime_scene)
    except (TypeError, ValueError) as exc:
        return ["runtime scene: %s" % exc]
    if actual["map_object_path"] != expected["map_object_path"]:
        errors.append("map_object_path mismatch")
    if actual["city_buildings"] != expected["city_buildings"]:
        errors.append("city building inventory mismatch")
    expected_owned = [item["plot_id"] for item in expected["plots"] if item["owned"]]
    actual_owned = [item["plot_id"] for item in actual["plots"] if item["owned"]]
    if actual_owned != expected_owned or actual["plots"] != expected["plots"]:
        errors.append("OwnedPlotIds/plot projection mismatch")
    if actual["placed_buildings"] != expected["placed_buildings"]:
        errors.append("Buildings projection mismatch")
    return errors


def _json_value_errors(value, prefix):
    errors = []
    if value is None or isinstance(value, (str, bool, int)):
        return errors
    if isinstance(value, float):
        if not math.isfinite(value):
            errors.append(prefix + " must be finite")
        return errors
    if isinstance(value, list):
        for index, item in enumerate(value):
            errors.extend(_json_value_errors(item, "%s[%d]" % (prefix, index)))
        return errors
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                errors.append(prefix + " has a non-text key")
            else:
                errors.extend(_json_value_errors(item, prefix + "." + key))
        return errors
    return [prefix + " contains a non-JSON value"]


def _difference_paths(left, right, prefix=""):
    if isinstance(left, dict) and isinstance(right, dict):
        differences = []
        for key in sorted(set(left).union(right)):
            field = key if not prefix else prefix + "." + key
            if key not in left or key not in right:
                differences.append(field)
            else:
                differences.extend(_difference_paths(left[key], right[key], field))
        return differences
    if isinstance(left, list) and isinstance(right, list):
        if len(left) != len(right):
            return [prefix]
        differences = []
        for index, (left_item, right_item) in enumerate(zip(left, right)):
            differences.extend(
                _difference_paths(left_item, right_item, "%s[%d]" % (prefix, index))
            )
        return differences
    return [] if left == right else [prefix]


def cf1_active_triplet_errors(a_pre, candidate, a_post):
    errors = []
    for label, value in (("A_PRE", a_pre), ("B", candidate), ("A_POST", a_post)):
        if not isinstance(value, dict):
            errors.append(label + " active readback must be an object")
            continue
        errors.extend(_json_value_errors(value, label))
    if errors:
        return errors
    for path in _difference_paths(a_pre, a_post):
        errors.append("A_PRE/A_POST mismatch at " + path)
    allowed_prefix = "grading.color_saturation_midtones.value"
    candidate_differences = _difference_paths(a_pre, candidate)
    for path in candidate_differences:
        if not path.startswith(allowed_prefix + "["):
            errors.append("unexpected A/B difference at " + path)
    try:
        a_midtones = a_pre["grading"]["color_saturation_midtones"]
        b_midtones = candidate["grading"]["color_saturation_midtones"]
        post_midtones = a_post["grading"]["color_saturation_midtones"]
        if a_midtones.get("override") is not True or b_midtones.get("override") is not True:
            errors.append("grading.color_saturation_midtones.override must remain true")
        if post_midtones.get("override") is not True:
            errors.append("A_POST grading.color_saturation_midtones.override must remain true")
        if a_midtones.get("value") != [1.0, 1.0, 1.0, 1.0]:
            errors.append("A_PRE grading.color_saturation_midtones.value mismatch")
        if b_midtones.get("value") != [1.15, 1.15, 1.15, 1.0]:
            errors.append("B grading.color_saturation_midtones.value mismatch")
        if post_midtones.get("value") != [1.0, 1.0, 1.0, 1.0]:
            errors.append("A_POST grading.color_saturation_midtones.value mismatch")
    except (KeyError, TypeError, AttributeError) as exc:
        errors.append("grading.color_saturation_midtones is malformed: %s" % exc)
    return errors


def capture_phase_plan(variant):
    common = ("APPLY_VARIANT", "SETTLE_VARIANT", "READBACK", "CAPTURE", "WAIT")
    if variant == "A_PRE":
        return ("PREPARE_TIME", "SETTLE_TIME_LIVE", "FREEZE") + common
    if variant in ("B", "A_POST"):
        return common
    raise ValueError("unknown variant: " + str(variant))


def next_capture_phase(variant, current_phase):
    plan = capture_phase_plan(variant)
    try:
        index = plan.index(current_phase)
    except ValueError as exc:
        raise ValueError("phase is not valid for variant") from exc
    return plan[index + 1] if index + 1 < len(plan) else None


def restore_phase_plan():
    return ("RESTORE_PREPARE", "RESTORE_SETTLE_LIVE", "RESTORE_FINAL")


def complete_live_settle(read_live_state, set_frozen_dilation):
    live_readback = read_live_state()
    set_frozen_dilation()
    return live_readback


def snapshot_rotation_values(rotation):
    return {
        "pitch": float(rotation.pitch),
        "yaw": float(rotation.yaw),
        "roll": float(rotation.roll),
    }


def build_fresh_rotator(rotation_values, rotator_factory):
    return rotator_factory(
        roll=rotation_values["roll"],
        pitch=rotation_values["pitch"],
        yaw=rotation_values["yaw"],
    )


def normalize_axis(degrees):
    normalized = math.fmod(float(degrees), 360.0)
    if normalized < 0.0:
        normalized += 360.0
    if normalized > 180.0:
        normalized -= 360.0
    return normalized


def restore_then_pin_numeric_rotation(
    restore_action, pin_action, readback_action
):
    restore_errors = list(restore_action() or [])
    try:
        pin_action()
    except Exception as pin_error:
        restore_errors.append(
            "directional relative rotation: %s" % pin_error
        )
    return restore_errors, readback_action()


def raise_after_diagnostic(primary_error, persist_action, cleanup_errors):
    try:
        persist_action()
    except Exception as diagnostic_error:
        cleanup_errors.append("failure diagnostic: %s" % diagnostic_error)
    raise primary_error


def build_failure_diagnostic(run_id, failure_context):
    return {
        "ok": False,
        "run_id": run_id,
        "restore_pending": True,
        "phase": failure_context["phase"],
        "sequence_index": failure_context["sequence_index"],
        "label": failure_context["label"],
        "attempted_readback": failure_context.get("attempted_readback"),
        "field_differences": failure_context.get("field_differences", []),
        "failure_context": failure_context,
    }


def readback_differences(actual, expected, tolerance=0.001, prefix=""):
    differences = []
    if isinstance(expected, dict):
        actual_dict = actual if isinstance(actual, dict) else {}
        for key in sorted(expected):
            field = key if not prefix else prefix + "." + key
            differences.extend(
                readback_differences(
                    actual_dict.get(key), expected[key], tolerance, field
                )
            )
        return differences
    if isinstance(expected, (list, tuple)):
        if type(actual) is not type(expected):
            return [{
                "field": prefix,
                "expected": expected,
                "actual": actual,
            }]
        shared_length = min(len(actual), len(expected))
        for index in range(shared_length):
            field = "%s[%d]" % (prefix, index)
            differences.extend(
                readback_differences(
                    actual[index], expected[index], tolerance, field
                )
            )
        for index in range(shared_length, len(expected)):
            differences.append({
                "field": "%s[%d]" % (prefix, index),
                "expected": expected[index],
                "actual": None,
            })
        for index in range(shared_length, len(actual)):
            differences.append({
                "field": "%s[%d]" % (prefix, index),
                "expected": None,
                "actual": actual[index],
            })
        return differences
    if isinstance(expected, bool):
        matches = actual is expected
    elif isinstance(expected, (int, float)) and not isinstance(expected, bool):
        try:
            matches = abs(float(actual) - float(expected)) <= tolerance
        except (TypeError, ValueError):
            matches = False
    else:
        matches = actual == expected
    if not matches:
        differences.append({
            "field": prefix,
            "expected": expected,
            "actual": actual,
        })
    return differences


def directional_restore_differences(actual, expected, tolerance=0.001):
    actual_dict = actual if isinstance(actual, dict) else {}
    angle_fields = {"pitch", "yaw", "roll", "effective_azimuth"}
    differences = []
    for field in sorted(expected):
        field_name = "directional." + field
        actual_value = actual_dict.get(field)
        expected_value = expected[field]
        if field not in angle_fields:
            differences.extend(
                readback_differences(
                    actual_value, expected_value, tolerance, field_name
                )
            )
            continue
        try:
            matches = abs(
                normalize_axis(float(actual_value) - float(expected_value))
            ) <= tolerance
        except (TypeError, ValueError, OverflowError):
            matches = False
        if not matches:
            differences.append({
                "field": field_name,
                "expected": expected_value,
                "actual": actual_value,
            })
    return differences


def atomic_write_json(path, payload):
    temp_path = path + ".tmp"
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(temp_path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(
            payload,
            stream,
            ensure_ascii=True,
            indent=2,
            sort_keys=True,
            allow_nan=False,
        )
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(temp_path, path)


def atomic_write_json_if_absent(path, payload):
    directory = os.path.dirname(os.path.abspath(path))
    os.makedirs(directory, exist_ok=True)
    temp_path = "%s.publish-%s.tmp" % (path, uuid.uuid4().hex)
    try:
        with open(temp_path, "x", encoding="utf-8", newline="\n") as stream:
            json.dump(
                payload,
                stream,
                ensure_ascii=True,
                indent=2,
                sort_keys=True,
                allow_nan=False,
            )
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.link(temp_path, path)
    finally:
        if os.path.lexists(temp_path):
            os.remove(temp_path)
