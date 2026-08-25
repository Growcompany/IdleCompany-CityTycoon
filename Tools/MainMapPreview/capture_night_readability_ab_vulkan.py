# -*- coding: utf-8 -*-
"""Capture N1/N2 MainMap night-readability variants under Vulkan ES3.1."""

import builtins
import json
import os
import re
import traceback

import unreal


MARKER = "[CGR-MAINMAP-NIGHT-AB-VULKAN] "
ACTIVE_HANDLE = "_CGR_MAINMAP_NIGHT_AB_VULKAN_CAPTURE_HANDLE"
WIDTH = 1600
HEIGHT = 900
SUNSET_HOUR = 19
NIGHT_DIRECTIONAL_TEMPERATURE = 9000.0
NIGHT_AMBIENT_COLOR = (0.3763, 0.4564, 0.5776, 1.0)
VARIANTS = {
    "N1": {
        "sky_night_intensity": 0.20,
        "night_directional_intensity": 0.08,
        "night_exposure_bias": 0.50,
        "window_emissive_scale": 0.75,
        "bloom_intensity": 0.30,
        "bloom_threshold": 3.0,
    },
    "N2": {
        "sky_night_intensity": 0.26,
        "night_directional_intensity": 0.12,
        "night_exposure_bias": 0.65,
        "window_emissive_scale": 0.65,
        "bloom_intensity": 0.30,
        "bloom_threshold": 3.0,
    },
}
TIMES = (
    ("1930", 19, 30),
    ("0000", 0, 0),
    ("0530", 5, 30),
)
ATMOSPHERE_CHECKS = (
    ("1200", 12, 0, True),
    ("0655", 6, 55, False),
    ("0705", 7, 5, True),
    ("1855", 18, 55, True),
    ("1905", 19, 5, False),
)
SEQUENCE = [
    (variant, time_label, hour, minute)
    for variant in ("N1", "N2")
    for time_label, hour, minute in TIMES
]
OUTPUT_DIR = os.path.normpath(
    os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "BuildingLookdev",
        "MainMapPreview",
        "NightReadabilityAB",
        "VulkanES31",
    )
)


def _game_world():
    try:
        world = unreal.get_editor_subsystem(
            unreal.UnrealEditorSubsystem
        ).get_game_world()
        if (
            world
            and "MainMap_TheRiverwalkCity" in world.get_path_name()
            and unreal.GameplayStatics.get_player_controller(world, 0)
        ):
            return world
    except Exception:
        pass

    for candidate in unreal.ObjectIterator(unreal.World):
        try:
            if (
                "MainMap_TheRiverwalkCity" in candidate.get_path_name()
                and unreal.GameplayStatics.get_player_controller(candidate, 0)
            ):
                return candidate
        except Exception:
            continue
    raise RuntimeError("MainMap game world was not found")


def _single_actor(world, actor_class, label):
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, actor_class)
    if len(actors) != 1:
        raise RuntimeError("expected one %s, found %d" % (label, len(actors)))
    return actors[0]


def _time_code(hour, minute):
    value = unreal.TimeCycleCode()
    value.hours = hour
    value.minutes = minute
    value.seconds = 0
    return value


def _time_dict(value):
    return {
        "hours": int(value.hours),
        "minutes": int(value.minutes),
        "seconds": int(value.seconds),
    }


def _capture_path(variant, time_label):
    return os.path.normpath(
        os.path.join(
            OUTPUT_DIR,
            "MainMap_%s_%s.png" % (variant, time_label),
        )
    )


def _building_keys(world):
    keys = set()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        match = re.match(r"BP_MB(\d+)", actor.get_class().get_name())
        if match:
            keys.add(int(match.group(1)))
    return keys


def _world_directional_lights(world):
    result = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(
        world, unreal.Actor
    ):
        result.extend(
            actor.get_components_by_class(unreal.DirectionalLightComponent)
        )
    return result


def _directional_readback(component):
    return {
        "cast_shadows": bool(component.get_editor_property("cast_shadows")),
        "atmosphere_sun_light": bool(
            component.get_editor_property("atmosphere_sun_light")
        ),
    }


def _snapshot_window_emissive(world):
    snapshots = []
    seen = set()
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if not re.match(r"BP_MB\d+", actor.get_class().get_name()):
            continue
        for component in actor.get_components_by_class(unreal.StaticMeshComponent):
            for material in component.get_materials():
                if not isinstance(material, unreal.MaterialInstanceDynamic):
                    continue
                identity = material.get_path_name()
                if identity in seen:
                    continue
                color = material.get_vector_parameter_value("Window_Emissive_Color")
                if max(float(color.r), float(color.g), float(color.b)) <= 0.001:
                    continue
                seen.add(identity)
                snapshots.append(
                    (
                        material,
                        unreal.LinearColor(color.r, color.g, color.b, color.a),
                    )
                )
    if not snapshots:
        raise RuntimeError("runtime facade window emissive MIDs were not found")
    return snapshots


def _scale_window_emissive(snapshots, scale):
    for material, original in snapshots:
        material.set_vector_parameter_value(
            "Window_Emissive_Color",
            unreal.LinearColor(
                original.r * scale,
                original.g * scale,
                original.b * scale,
                original.a,
            ),
        )


def _verify_window_emissive(snapshots, scale):
    max_error = 0.0
    for material, original in snapshots:
        actual = material.get_vector_parameter_value("Window_Emissive_Color")
        expected = (
            original.r * scale,
            original.g * scale,
            original.b * scale,
            original.a,
        )
        errors = (
            abs(float(actual.r) - expected[0]),
            abs(float(actual.g) - expected[1]),
            abs(float(actual.b) - expected[2]),
            abs(float(actual.a) - expected[3]),
        )
        max_error = max(max_error, *errors)
    if max_error > 0.001:
        raise RuntimeError(
            "window emissive readback mismatch: max error %.6f" % max_error
        )
    return {
        "window_emissive_scale": scale,
        "window_emissive_mids_verified": len(snapshots),
        "window_emissive_max_abs_error": max_error,
    }


def _set_bloom(post_process, variant):
    settings = post_process.get_editor_property("settings")
    settings.set_editor_property("override_bloom_intensity", True)
    settings.set_editor_property("bloom_intensity", variant["bloom_intensity"])
    settings.set_editor_property("override_bloom_threshold", True)
    settings.set_editor_property("bloom_threshold", variant["bloom_threshold"])
    post_process.set_editor_property("settings", settings)


def _set_sunset(manager):
    if not manager.apply_transient_sun_set_time_for_preview(
        _time_code(SUNSET_HOUR, 0)
    ):
        raise RuntimeError("transient preview sunset was rejected")
    return _time_dict(manager.get_sun_set_time())


def _apply_variant(sky, post_process, window_snapshots, variant_name):
    variant = VARIANTS[variant_name]
    night_look = sky.get_night_look()
    night_look.sky_night_intensity = variant["sky_night_intensity"]
    night_look.night_directional_intensity = variant[
        "night_directional_intensity"
    ]
    night_look.night_directional_temperature = NIGHT_DIRECTIONAL_TEMPERATURE
    night_look.night_ambient_color = unreal.LinearColor(*NIGHT_AMBIENT_COLOR)
    night_look.night_exposure_bias = variant["night_exposure_bias"]
    if not sky.apply_transient_night_look(night_look):
        raise RuntimeError("transient night look was rejected: " + variant_name)
    _set_bloom(post_process, variant)
    _scale_window_emissive(window_snapshots, variant["window_emissive_scale"])

    applied_look = sky.get_night_look()
    settings = post_process.get_editor_property("settings")
    readback = {
        "sky_night_intensity": float(applied_look.sky_night_intensity),
        "night_directional_intensity": float(
            applied_look.night_directional_intensity
        ),
        "night_directional_temperature": float(
            applied_look.night_directional_temperature
        ),
        "night_ambient_color": [
            float(applied_look.night_ambient_color.r),
            float(applied_look.night_ambient_color.g),
            float(applied_look.night_ambient_color.b),
            float(applied_look.night_ambient_color.a),
        ],
        "night_exposure_bias": float(applied_look.night_exposure_bias),
        "bloom_intensity": float(settings.get_editor_property("bloom_intensity")),
        "bloom_threshold": float(settings.get_editor_property("bloom_threshold")),
    }
    readback.update(
        _verify_window_emissive(
            window_snapshots, variant["window_emissive_scale"]
        )
    )

    for name in (
        "sky_night_intensity",
        "night_directional_intensity",
        "night_exposure_bias",
        "bloom_intensity",
        "bloom_threshold",
    ):
        if abs(readback[name] - variant[name]) > 0.001:
            raise RuntimeError(
                "%s %s readback mismatch: %s" % (
                    variant_name,
                    name,
                    readback[name],
                )
            )
    if abs(readback["night_directional_temperature"] - NIGHT_DIRECTIONAL_TEMPERATURE) > 0.001:
        raise RuntimeError("night directional temperature readback mismatch")
    if any(
        abs(actual - expected) > 0.001
        for actual, expected in zip(
            readback["night_ambient_color"], NIGHT_AMBIENT_COLOR
        )
    ):
        raise RuntimeError("night ambient color readback mismatch")
    return readback


world = _game_world()
controller = unreal.GameplayStatics.get_player_controller(world, 0)
if not controller:
    raise RuntimeError("MainMap player controller was not found")

command_line = unreal.SystemLibrary.get_command_line()
if "-vulkan" not in command_line.lower():
    raise RuntimeError("capture process was not launched with -vulkan")
if "-featureleveles31" not in command_line.lower():
    raise RuntimeError("capture process was not launched with -FeatureLevelES31")

# Win64 ES3.1 preview does not inherit the Android device profile. Mirror the
# shipping Android profile so the A/B uses the same deterministic manual exposure.
unreal.SystemLibrary.execute_console_command(
    world, "r.EyeAdaptation.MethodOverride 2", controller
)
if (
    unreal.SystemLibrary.get_console_variable_int_value(
        "r.EyeAdaptation.MethodOverride"
    )
    != 2
):
    raise RuntimeError("failed to apply the Android manual-exposure override")

os.makedirs(OUTPUT_DIR, exist_ok=True)
for variant_name, time_label, _hour, _minute in SEQUENCE:
    output_path = _capture_path(variant_name, time_label)
    if os.path.exists(output_path):
        os.remove(output_path)

old_handle = getattr(builtins, ACTIVE_HANDLE, None)
if old_handle is not None:
    unreal.unregister_slate_post_tick_callback(old_handle)

state = {
    "phase": "warmup",
    "ticks": 0,
    "sequence_index": 0,
    "wait_ticks": 0,
    "sky": None,
    "manager": None,
    "post_process": None,
    "window_snapshots": None,
    "cycle_duration_before": None,
    "sunset_before": None,
    "sunset_applied": None,
    "day_values": None,
    "building_keys": None,
    "directional_light": None,
    "directional_light_component": None,
    "atmosphere_check_index": 0,
    "atmosphere_readbacks": {},
    "capture_light_readbacks": {},
    "applied_variants": {},
}


def _initialize():
    sky = _single_actor(world, unreal.TimeCycleSky, "TimeCycleSky")
    manager = _single_actor(world, unreal.TimeCycleManager, "TimeCycleManager")
    post_processes = [
        actor
        for actor in unreal.GameplayStatics.get_all_actors_of_class(
            world, unreal.PostProcessVolume
        )
        if actor.get_editor_property("unbound")
    ]
    if len(post_processes) != 1:
        raise RuntimeError(
            "expected one unbound PostProcessVolume, found %d" % len(post_processes)
        )

    sky_directional_lights = sky.get_components_by_class(
        unreal.DirectionalLightComponent
    )
    world_directional_lights = _world_directional_lights(world)
    if len(sky_directional_lights) != 1:
        raise RuntimeError(
            "expected one TimeCycleSky DirectionalLight, found %d"
            % len(sky_directional_lights)
        )
    if len(world_directional_lights) != 1:
        raise RuntimeError(
            "expected one world DirectionalLight, found %d"
            % len(world_directional_lights)
        )
    directional_light = sky_directional_lights[0]
    directional_state = {
        "sky_count": len(sky_directional_lights),
        "world_count": len(world_directional_lights),
    }
    directional_state.update(_directional_readback(directional_light))
    if directional_state["cast_shadows"]:
        raise RuntimeError("TimeCycleSky DirectionalLight must not cast shadows")

    cycle_duration = manager.get_cycle_duration()
    cycle_dict = _time_dict(cycle_duration)
    if cycle_dict != {"hours": 0, "minutes": 0, "seconds": 24}:
        raise RuntimeError("placed CycleDuration is not 24 seconds: %r" % cycle_dict)

    day_look = sky.get_day_look()
    day_values = {
        "sun_max_intensity": float(day_look.sun_max_intensity),
        "sky_day_intensity": float(day_look.sky_day_intensity),
        "day_exposure_bias": float(day_look.day_exposure_bias),
    }
    expected_day = {
        "sun_max_intensity": 0.8,
        "sky_day_intensity": 0.5,
        "day_exposure_bias": -0.15,
    }
    for name, expected in expected_day.items():
        if abs(day_values[name] - expected) > 0.001:
            raise RuntimeError("placed %s changed: %s" % (name, day_values[name]))

    keys = _building_keys(world)
    if len(keys) != 40:
        raise RuntimeError("expected 40 MainMap buildings, found %d" % len(keys))

    manager.pause_cycle()
    sunset_before = _time_dict(manager.get_sun_set_time())
    sunset_applied = _set_sunset(manager)
    if sunset_applied != {"hours": SUNSET_HOUR, "minutes": 0, "seconds": 0}:
        raise RuntimeError("transient sunset was not applied: %r" % sunset_applied)
    state["sky"] = sky
    state["manager"] = manager
    state["post_process"] = post_processes[0]
    state["window_snapshots"] = _snapshot_window_emissive(world)
    state["cycle_duration_before"] = cycle_dict
    state["sunset_before"] = sunset_before
    state["sunset_applied"] = sunset_applied
    state["day_values"] = day_values
    state["building_keys"] = len(keys)
    state["directional_light"] = directional_state
    state["directional_light_component"] = directional_light


def _finish(ok, error=None):
    handle = getattr(builtins, ACTIVE_HANDLE, None)
    if handle is not None:
        unreal.unregister_slate_post_tick_callback(handle)
        setattr(builtins, ACTIVE_HANDLE, None)

    manager = state.get("manager")
    cycle_duration_after = None
    sunset_after = None
    if manager:
        cycle_duration_after = _time_dict(manager.get_cycle_duration())
        sunset_after = _time_dict(manager.get_sun_set_time())

    captures = {}
    for variant_name, time_label, _hour, _minute in SEQUENCE:
        label = "%s_%s" % (variant_name, time_label)
        path = _capture_path(variant_name, time_label)
        captures[label] = {
            "path": path,
            "exists": os.path.exists(path),
            "bytes": os.path.getsize(path) if os.path.exists(path) else 0,
        }

    result = {
        "ok": ok,
        "world": world.get_path_name(),
        "command_line": command_line,
        "cvars": {
            name: unreal.SystemLibrary.get_console_variable_int_value(name)
            for name in (
                "r.Mobile.ShadingPath",
                "r.MobileHDR",
                "r.EyeAdaptation.MethodOverride",
            )
        },
        "building_keys": state.get("building_keys"),
        "window_emissive_mids": len(state.get("window_snapshots") or []),
        "directional_light": state.get("directional_light"),
        "atmosphere_readbacks": state.get("atmosphere_readbacks"),
        "capture_light_readbacks": state.get("capture_light_readbacks"),
        "cycle_duration_before": state.get("cycle_duration_before"),
        "cycle_duration_after": cycle_duration_after,
        "sunset_before": state.get("sunset_before"),
        "sunset_applied": state.get("sunset_applied"),
        "sunset_after": sunset_after,
        "preserved_day_values": state.get("day_values"),
        "sunset_hour": SUNSET_HOUR,
        "night_directional_temperature": NIGHT_DIRECTIONAL_TEMPERATURE,
        "night_ambient_color": NIGHT_AMBIENT_COLOR,
        "variants": VARIANTS,
        "applied_variants": state.get("applied_variants"),
        "captures": captures,
    }
    if error:
        result["error"] = error
    line = MARKER + json.dumps(result, ensure_ascii=True, separators=(",", ":"))
    print(line)
    (unreal.log if ok else unreal.log_error)(line)
    unreal.SystemLibrary.execute_console_command(world, "quit", controller)


def _tick(_delta_seconds):
    try:
        state["ticks"] += 1
        if state["phase"] == "warmup":
            if state["ticks"] < 600:
                return
            _initialize()
            state["phase"] = "atmosphere_apply"
            state["ticks"] = 0
            return

        if state["phase"] == "atmosphere_apply":
            check_label, hour, minute, _expected = ATMOSPHERE_CHECKS[
                state["atmosphere_check_index"]
            ]
            state["manager"].set_current_time(_time_code(hour, minute))
            state["phase"] = "atmosphere_settle"
            state["ticks"] = 0
            return

        if state["phase"] == "atmosphere_settle":
            if state["ticks"] < 60:
                return
            check_label, _hour, _minute, expected = ATMOSPHERE_CHECKS[
                state["atmosphere_check_index"]
            ]
            readback = _directional_readback(
                state["directional_light_component"]
            )
            state["atmosphere_readbacks"][check_label] = readback
            if readback["cast_shadows"]:
                raise RuntimeError(
                    "DirectionalLight shadows enabled at " + check_label
                )
            if readback["atmosphere_sun_light"] != expected:
                raise RuntimeError(
                    "Atmosphere Sun Light mismatch at %s: %s"
                    % (check_label, readback["atmosphere_sun_light"])
                )
            state["atmosphere_check_index"] += 1
            state["ticks"] = 0
            if state["atmosphere_check_index"] < len(ATMOSPHERE_CHECKS):
                state["phase"] = "atmosphere_apply"
            else:
                state["phase"] = "apply"
            return

        if state["sequence_index"] >= len(SEQUENCE):
            missing = [
                _capture_path(variant_name, time_label)
                for variant_name, time_label, _hour, _minute in SEQUENCE
                if not os.path.exists(_capture_path(variant_name, time_label))
            ]
            if missing:
                raise RuntimeError("missing captures: " + ", ".join(missing))
            _finish(True)
            return

        variant_name, time_label, hour, minute = SEQUENCE[
            state["sequence_index"]
        ]
        output_path = _capture_path(variant_name, time_label)

        if state["phase"] == "apply":
            if state["sequence_index"] == 0 or SEQUENCE[
                state["sequence_index"] - 1
            ][0] != variant_name:
                state["applied_variants"][variant_name] = _apply_variant(
                    state["sky"],
                    state["post_process"],
                    state["window_snapshots"],
                    variant_name,
                )
            state["manager"].set_current_time(_time_code(hour, minute))
            state["phase"] = "settle"
            state["ticks"] = 0
            return

        if state["phase"] == "settle":
            if state["ticks"] < 180:
                return
            light_readback = _directional_readback(
                state["directional_light_component"]
            )
            capture_label = "%s_%s" % (variant_name, time_label)
            state["capture_light_readbacks"][capture_label] = light_readback
            if light_readback["cast_shadows"]:
                raise RuntimeError(
                    "DirectionalLight shadows enabled at " + capture_label
                )
            if light_readback["atmosphere_sun_light"]:
                raise RuntimeError(
                    "folded night fill drove Atmosphere at " + capture_label
                )
            command = 'HighResShot %dx%d filename="%s"' % (
                WIDTH,
                HEIGHT,
                output_path.replace("\\", "/"),
            )
            unreal.SystemLibrary.execute_console_command(world, command, controller)
            state["phase"] = "wait"
            state["wait_ticks"] = 0
            return

        if state["phase"] == "wait":
            state["wait_ticks"] += 1
            if not os.path.exists(output_path):
                if state["wait_ticks"] > 1800:
                    raise RuntimeError("capture timeout: " + output_path)
                return
            state["sequence_index"] += 1
            state["phase"] = "apply"
            state["ticks"] = 0
            return

        raise RuntimeError("unknown phase: " + str(state["phase"]))
    except Exception as exc:
        unreal.log_error(traceback.format_exc())
        _finish(False, "%s: %s" % (type(exc).__name__, exc))


handle = unreal.register_slate_post_tick_callback(_tick)
setattr(builtins, ACTIVE_HANDLE, handle)
print(MARKER + "started")
unreal.log(MARKER + "started")
