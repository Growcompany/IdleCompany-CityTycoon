"""Pure experiment contract for the MainMap mobile PC-like visual A/B series."""

import re
from collections.abc import Sequence

from mobile_cinematic_filter import (
    BASELINE_SAFETY_TIMES,
    CF1_TIMES,
    cf1_expected_lighting,
    cf1_profile_for as _cf1_profile_for,
)


TIMES = ("0900", "1200", "1600")
ORDER = (
    "E1", "E2", "E3", "E4", "C1", "L1", "S1", "S2", "S3", "R1", "L2",
    "CF1", "E5", "E6"
)
SEMANTIC_EXPERIMENTS = ("S1", "S2", "R1")
BASELINE = {
    "sky_light_intensity": 0.50,
    "wall_color_scale": 0.78,
    "wall_chroma": 1.0,
    "windows_roughness": 0.40,
    "normal_intensity": 0.08,
    "pc_match_lut_intensity": 0.0,
    "facade_look_transfer_strength": 0.0,
    "source_pbr_bake": False,
    "reflection_mode": "broad_emissive_add",
    "reflection_cubemap": (
        "/Game/CompanyGrowth/Resources/Materials/MainMap/"
        "TC_StudioReflect.TC_StudioReflect"
    ),
    "reflection_capture": False,
}
CANDIDATES = {
    "E1": ("sky_light_intensity", 0.35),
    "E2": ("wall_color_scale", 0.92),
    "E3": ("windows_roughness", 0.30),
    "E4": ("normal_intensity", 0.14),
    "C1": ("wall_chroma", 1.12),
    "L1": ("pc_match_lut_intensity", 0.35),
    "S1": ("facade_look_transfer_strength", 1.0),
    "S2": ("facade_look_transfer_strength", 1.0),
    "S3": ("source_pbr_bake", True),
    "R1": ("reflection_mode", "baked_energy_lerp"),
    "L2": ("pc_match_lut_intensity", 1.0),
    "CF1": ("color_saturation_midtones", (1.15, 1.15, 1.15, 1.0)),
    "E5": ("reflection_mode", "window_mask_energy_lerp"),
    "E6": ("reflection_capture", True),
}
CAPTURE_SUPPORTED = (
    "E1", "E2", "E3", "E4", "C1", "L1", "S1", "S2", "S3", "R1", "L2",
    "CF1"
)
MIN_LIVE_SETTLE_TICKS = 180
MAX_LIVE_SETTLE_TICKS = 600
REQUIRED_STABLE_SAMPLES = 3
LIVE_TIME_DILATION = 1.0
FROZEN_TIME_DILATION = 0.0001
VARIANT_SETTLE_TICKS = 120
RESTORE_LIVE_TICKS = 30
CAPTURE_EXPOSURE_BIAS = -0.15
PC_MATCH_LUT_OBJECT_PATH = (
    "/Game/__CGRTransient/MobileVisualAB/"
    "T_PCMatch_Day_LUT.T_PCMatch_Day_LUT"
)
SEMANTIC_SKIN_MASTER_OBJECT_PATHS = {
    "Complex": (
        "/Game/__CGRTransient/MobileVisualAB/SemanticSkin/"
        "M_MB_Complex_S1.M_MB_Complex_S1"
    ),
    "Simple": (
        "/Game/__CGRTransient/MobileVisualAB/SemanticSkin/"
        "M_MB_Simple_S1.M_MB_Simple_S1"
    ),
}
DIRECTIONAL_EXPECTATIONS = {
    "0900": -36.0,
    "1200": -90.0,
    "1600": -38.57142639160157,
}


def _flag_values(command_line, name):
    pattern = r"(?:^|\s)-" + re.escape(name) + r"=([^\s]*)"
    return re.findall(pattern, command_line, flags=re.IGNORECASE)


def experiment_from_command_line(command_line: str) -> str:
    values = _flag_values(command_line, "CGRExperiment")
    if not values:
        raise ValueError("CGRExperiment is required")
    if len(values) != 1:
        raise ValueError("CGRExperiment must appear at most once")
    experiment = values[0].upper()
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + values[0])
    return experiment


def saved_suffix_for(experiment: str) -> str:
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + experiment)
    return "MainMapMobilePCLike%sAB" % experiment


def ensure_capture_supported(experiment: str) -> str:
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + experiment)
    if experiment not in CAPTURE_SUPPORTED:
        raise ValueError(
            "%s capture is unsupported until its dedicated task" % experiment
        )
    return experiment


def load_pc_match_lut_for_experiment(
    experiment: str, loader, adopted: Sequence[str] = ()
):
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + experiment)
    if experiment == "CF1":
        if tuple(adopted) != ("E2", "L2"):
            raise ValueError("CF1 requires exact adopted baseline E2,L2")
        return loader(PC_MATCH_LUT_OBJECT_PATH)
    if experiment in ("S2", "S3", "R1", "L2") and tuple(adopted) != ("E2", "L1"):
        raise ValueError("%s requires exact adopted baseline E2,L1" % experiment)
    if experiment == "S1" and "L1" not in adopted:
        raise ValueError("S1 requires adopted L1")
    if experiment != "L1" and "L1" not in adopted:
        return None
    return loader(PC_MATCH_LUT_OBJECT_PATH)


def load_semantic_skin_masters_for_experiment(experiment: str, loader):
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + experiment)
    if experiment not in SEMANTIC_EXPERIMENTS:
        return None
    return {
        kind: loader(path)
        for kind, path in SEMANTIC_SKIN_MASTER_OBJECT_PATHS.items()
    }


def launch_contract_from_command_line(command_line: str) -> tuple[str, Sequence[str]]:
    probe_values = _flag_values(command_line, "CGRProbe")
    if len(probe_values) != 1 or probe_values[0].lower() != "mobilepclikeab":
        raise ValueError("exactly one -CGRProbe=MobilePCLikeAB is required")
    adopted_values = _flag_values(command_line, "CGRAdopted")
    if len(adopted_values) != 1:
        raise ValueError("exactly one -CGRAdopted flag is required")
    experiment = experiment_from_command_line(command_line)
    adopted = tuple(adopted_from_command_line(command_line))
    profile_for(
        experiment,
        "A_PRE",
        adopted,
        "0900" if experiment == "CF1" else None,
    )
    suffix_values = _flag_values(command_line, "saveddirsuffix")
    expected_suffix = saved_suffix_for(experiment)
    if len(suffix_values) != 1 or suffix_values[0].lower() != expected_suffix.lower():
        raise ValueError("exactly one matching -saveddirsuffix is required")
    return experiment, adopted


def adopted_from_command_line(command_line: str) -> Sequence[str]:
    values = _flag_values(command_line, "CGRAdopted")
    if not values:
        return ()
    if len(values) != 1:
        raise ValueError("CGRAdopted must appear at most once")
    if values[0] == "":
        return ()
    adopted = tuple(item.upper() for item in values[0].split(","))
    if (
        any(item not in ORDER for item in adopted)
        or len(set(adopted)) != len(adopted)
        or tuple(sorted(adopted, key=ORDER.index)) != adopted
    ):
        raise ValueError("adopted experiments must be a unique ordered subset")
    return adopted


def profile_for(
    experiment: str,
    variant: str,
    adopted: Sequence[str],
    time_code: str | None = None,
) -> dict:
    if experiment not in ORDER or variant not in ("A_PRE", "B", "A_POST"):
        raise ValueError((experiment, variant))
    if experiment == "CF1" and tuple(adopted) != ("E2", "L2"):
        raise ValueError("CF1 requires exact adopted baseline E2,L2")
    current_index = ORDER.index(experiment)
    expected_order = tuple(item for item in ORDER[:current_index] if item in adopted)
    if len(set(adopted)) != len(adopted) or tuple(adopted) != expected_order:
        raise ValueError("adopted experiments must be an ordered prior subset")
    if experiment in ("S2", "S3", "R1", "L2") and tuple(adopted) != ("E2", "L1"):
        raise ValueError("%s requires exact adopted baseline E2,L1" % experiment)
    if experiment == "S1" and "L1" not in adopted:
        raise ValueError("S1 requires adopted L1")
    result = dict(BASELINE)
    for item in adopted:
        key, value = CANDIDATES[item]
        result[key] = value
    if experiment == "CF1":
        if time_code is None:
            raise ValueError("CF1 requires an explicit time code")
        result.update(_cf1_profile_for(variant, time_code))
        return result
    if variant == "B":
        key, value = CANDIDATES[experiment]
        result[key] = value
    return result


def runtime_wall_multiplier(profile: dict) -> float:
    return float(profile["wall_color_scale"]) / float(BASELINE["wall_color_scale"])


def profile_wall_color(source: dict, profile: dict) -> dict:
    multiplier = runtime_wall_multiplier(profile)
    red = float(source["r"]) * multiplier
    green = float(source["g"]) * multiplier
    blue = float(source["b"]) * multiplier
    luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue
    chroma = float(profile["wall_chroma"])
    return {
        "r": luminance + (red - luminance) * chroma,
        "g": luminance + (green - luminance) * chroma,
        "b": luminance + (blue - luminance) * chroma,
        "a": float(source["a"]),
    }


def requires_live_time_settle(variant: str) -> bool:
    if variant not in ("A_PRE", "B", "A_POST"):
        raise ValueError("unknown variant: " + variant)
    return variant == "A_PRE"


def is_time_settled(live_ticks: int, stable_samples: int) -> bool:
    return (
        live_ticks >= MIN_LIVE_SETTLE_TICKS
        and stable_samples >= REQUIRED_STABLE_SAMPLES
    )


def is_time_settle_timed_out(live_ticks: int) -> bool:
    return live_ticks > MAX_LIVE_SETTLE_TICKS


def _experiment_and_time(experiment_or_time: str, time_code: str | None):
    if time_code is None:
        return None, experiment_or_time
    if experiment_or_time not in ORDER:
        raise ValueError("unknown experiment: " + experiment_or_time)
    return experiment_or_time, time_code


def expected_capture_exposure(
    experiment_or_time: str,
    time_code: str | None = None,
) -> dict:
    experiment, resolved_time = _experiment_and_time(
        experiment_or_time, time_code
    )
    if experiment == "CF1":
        bias = cf1_expected_lighting(resolved_time)["exposure_bias"]
    else:
        if resolved_time not in TIMES:
            raise ValueError("unknown time code: " + resolved_time)
        bias = CAPTURE_EXPOSURE_BIAS
    return {
        "method_override": True,
        "method": "<AutoExposureMethod.AEM_MANUAL: 2>",
        "bias_override": True,
        "bias": bias,
        "method_cvar": 2,
    }


def expected_directional(
    experiment_or_time: str,
    time_code: str | None = None,
) -> dict:
    experiment, resolved_time = _experiment_and_time(
        experiment_or_time, time_code
    )
    if experiment == "CF1":
        lighting = cf1_expected_lighting(resolved_time)
        return {
            "pitch": lighting["component_pitch"],
            "effective_azimuth": lighting["effective_azimuth"],
            "intensity": lighting["directional_intensity"],
            "use_temperature": lighting["use_temperature"],
            "temperature": lighting["directional_temperature"],
            "cast_shadows": lighting["cast_shadows"],
            "cast_dynamic_shadows": lighting["cast_dynamic_shadows"],
        }
    try:
        pitch = DIRECTIONAL_EXPECTATIONS[resolved_time]
    except KeyError as exc:
        raise ValueError("unknown time code: " + resolved_time) from exc
    return {
        "pitch": pitch,
        "effective_azimuth": 35.0,
        "intensity": 0.8,
        "use_temperature": True,
        "temperature": 6500.0,
    }


def next_directional_stable_count(
    current_count: int,
    readback: dict,
    time_code: str,
    experiment: str | None = None,
) -> int:
    expected = (
        expected_directional(experiment, time_code)
        if experiment is not None
        else expected_directional(time_code)
    )
    tolerances = {
        "pitch": 0.5,
        "effective_azimuth": 0.02,
        "intensity": 0.001,
        "temperature": 0.5,
    }
    for field, target in expected.items():
        actual = readback.get(field)
        if field in (
            "use_temperature",
            "cast_shadows",
            "cast_dynamic_shadows",
        ):
            if actual is not target:
                return 0
        elif actual is None or abs(float(actual) - float(target)) > tolerances[field]:
            return 0
    return current_count + 1


def next_cf1_lighting_stable_count(
    current_count: int,
    readback: dict,
    time_code: str,
) -> int:
    if not isinstance(readback, dict):
        return 0
    directional = readback.get("directional")
    if not isinstance(directional, dict) or next_directional_stable_count(
        0, directional, time_code, experiment="CF1"
    ) != 1:
        return 0
    expected_lighting = cf1_expected_lighting(time_code)
    try:
        if abs(
            float(readback.get("sky_light_intensity"))
            - float(expected_lighting["sky_light_intensity"])
        ) > 0.001:
            return 0
    except (TypeError, ValueError):
        return 0
    sky_color = readback.get("sky_light_color")
    expected_sky_color = expected_lighting["sky_light_color"]
    if not isinstance(sky_color, (list, tuple)) or len(sky_color) != 4:
        return 0
    try:
        if any(
            abs(float(actual) - float(expected)) > 0.01
            for actual, expected in zip(sky_color, expected_sky_color)
        ):
            return 0
    except (TypeError, ValueError):
        return 0
    exposure = readback.get("exposure")
    expected_exposure = expected_capture_exposure("CF1", time_code)
    if not isinstance(exposure, dict):
        return 0
    for field in ("method_override", "method", "bias_override", "method_cvar"):
        if exposure.get(field) != expected_exposure[field]:
            return 0
    try:
        if abs(
            float(exposure.get("bias")) - float(expected_exposure["bias"])
        ) > 0.001:
            return 0
    except (TypeError, ValueError):
        return 0
    return current_count + 1


def runtime_adopted_for(
    experiment: str,
    current_stack: Sequence[str],
) -> tuple[str, ...]:
    expected_stack = ("N1DIM", "AZ35", "TCGB", "SH04", "E2", "L2")
    if experiment != "CF1":
        raise ValueError("runtime stack adaptation is defined only for CF1")
    if tuple(current_stack) != expected_stack:
        raise ValueError("CF1 requires the exact governed baseline stack")
    return ("E2", "L2")


def times_for(experiment: str, stage: str = "AB") -> tuple[str, ...]:
    if experiment not in ORDER:
        raise ValueError("unknown experiment: " + experiment)
    if experiment == "CF1":
        if stage == "BASELINE_SAFETY":
            return BASELINE_SAFETY_TIMES
        if stage == "AB":
            return tuple(CF1_TIMES)
        raise ValueError("unknown CF1 stage: " + stage)
    if stage != "AB":
        raise ValueError("legacy experiments support only the AB stage")
    return TIMES


def capture_sequence(
    experiment: str | None = None,
    stage: str = "AB",
) -> tuple:
    if experiment is None:
        experiment = "L2"
    times = times_for(experiment, stage)
    variants = (
        ("A_PRE",)
        if experiment == "CF1" and stage == "BASELINE_SAFETY"
        else ("A_PRE", "B", "A_POST")
    )
    return tuple(
        (variant, time_code)
        for time_code in times
        for variant in variants
    )
