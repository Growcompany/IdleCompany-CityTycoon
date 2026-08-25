"""Pure CF1 preview contract.

This module is intentionally ``preview_non_authoritative`` until the root
MainMap visual-governance registry exists.  Its render values are the approved
CF1 design literals; no production asset is mutated by this module.
"""

from __future__ import annotations

import copy
import hashlib
import json
import math
import pathlib
import uuid


AUTHORITY_MODE = "preview_non_authoritative"

CF1_TIMES = {
    "0900": (9, 0),
    "1800": (18, 0),
    "1930": (19, 30),
    "0000": (0, 0),
    "0530": (5, 30),
}
BASELINE_SAFETY_TIMES = ("1800", "1930", "0000", "0530")
N1_DIM_NIGHT_LOOK = {
    "sky_night_intensity": 0.20,
    "night_directional_intensity": 0.08,
    "night_directional_temperature": 9000.0,
    "night_ambient_color": (0.3763, 0.4564, 0.5776, 1.0),
    "night_exposure_bias": 0.20,
}
TIME_GRADE_B = {
    "0900": (1.00, 1.00, (1.00, 1.00, 1.00)),
    "1800": (1.04, 1.03, (1.04, 1.00, 0.94)),
    "1930": (0.92, 1.02, (1.02, 1.05, 1.10)),
    "0000": (0.92, 1.02, (1.02, 1.05, 1.10)),
    "0530": (1.03, 1.02, (1.025, 1.00, 0.96)),
}
BASELINE_SAFETY_LIMITS = {
    "1800": {
        "p50": (0.09, 0.16),
        "under_0p02_max": 20.0,
        "over_0p8_max": 0.5,
    },
    "1930": {
        "p50": (0.035, 0.070),
        "under_0p02_max": 40.0,
        "over_0p8_max": 1.5,
    },
    "0000": {
        "p50": (0.040, 0.080),
        "under_0p02_max": 40.0,
        "over_0p8_max": 1.5,
    },
    "0530": {
        "p50": (0.070, 0.130),
        "under_0p02_max": 30.0,
        "over_0p8_max": 1.0,
    },
}
L2_LUT_SOURCE_PATH = (
    "Tools/MainMapPreview/Baselines/L2/T_PCMatch_Day_LUT.png"
)
L2_LUT_SOURCE_SHA256 = (
    "b9d21a63b85f5f2da146d68d4b51e590"
    "eadcf693f347c5693cea391ca9a46dff"
)
BASELINE_REFERENCE_PATHS = {
    time_code: (
        "Saved/BuildingLookdev/MainMapPreview/TimeColorGradingAB/"
        "VulkanES31/MainMap_TimeColor_B_%s.png" % time_code
    )
    for time_code in BASELINE_SAFETY_TIMES
}
BASELINE_REFERENCE_SHA256 = {
    "1800": "1fe4e49c936983c85f027f39b75d63842d37aea78c3e8a49ac42a4eeb2199bd4",
    "1930": "c9c846e4f02484f282e5bc2f2574fd6866f05b005e20bc83a259bd819049dc06",
    "0000": "83cf35129acaf2fecc4f048e4021a560aecaea40eeed14fa60edbc3c179119c8",
    "0530": "14c72d65a37636072306ce8f384d5e0f96fc1ef7993fb1b24e0e3ccdded39c0b",
}
FULL_FRAME_ROI = (0, 0, 1600, 900)
BUILDING_ROI = (160, 25, 1400, 760)
A_A_MEAN_ABS_RGB_MAX = 0.001

_POLICY_PATH = pathlib.Path(__file__).with_name(
    "cf1_baseline_safety_policy.json"
)
_CF1_VARIANTS = ("A_PRE", "B", "A_POST")
_SUNRISE_HOUR = 7.0
_SUNSET_HOUR = 19.0
_DAWN_BLEND_HOURS = 2.0
_DUSK_BLEND_HOURS = 2.0
_SUN_MAX_INTENSITY = 0.8
_DAY_DIRECTIONAL_TEMPERATURE = 6500.0
_TRANSITION_DIRECTIONAL_TEMPERATURE = 1800.0
_SKY_DAY_INTENSITY = 0.5
_DAY_EXPOSURE_BIAS = -0.15
_UE54_SRGB_213 = 0.665387295591707
_BASE_SKY_COLOR = (
    _UE54_SRGB_213,
    _UE54_SRGB_213,
    _UE54_SRGB_213,
    1.0,
)
_DUSK_AMBIENT_COLOR = (1.0, 0.62, 0.38, 1.0)
_DUSK_AMBIENT_STRENGTH = 0.4
_SUN_AZIMUTH = 35.0

_MUTATION_CONTRACT = {
    "property": "PostProcessVolume.Settings.ColorSaturationMidtones",
    "baseline": [1, 1, 1, 1],
    "candidate": [1.15, 1.15, 1.15, 1],
    "time_set": ["0900", "1800", "1930", "0000", "0530"],
    "renderer": "Vulkan",
    "feature_level": "ES3_1",
    "map": "MainMap_TheRiverwalkCity",
}


def _lerp(start: float, end: float, alpha: float) -> float:
    return start + (end - start) * alpha


def _lerp_color(start: tuple, end: tuple, alpha: float) -> tuple:
    return tuple(_lerp(float(a), float(b), alpha) for a, b in zip(start, end))


def _mapped_range(
    start_time: float,
    end_time: float,
    start_angle: float,
    end_angle: float,
    current_time: float,
) -> float:
    alpha = (current_time - start_time) / (end_time - start_time)
    alpha = min(1.0, max(0.0, alpha))
    return _lerp(start_angle, end_angle, alpha)


def _raw_solar_pitch(hour: float) -> float:
    if _SUNRISE_HOUR <= hour <= 12.0:
        return _mapped_range(_SUNRISE_HOUR, 12.0, 0.0, -90.0, hour)
    if 12.0 <= hour <= _SUNSET_HOUR:
        return _mapped_range(12.0, _SUNSET_HOUR, -90.0, -180.0, hour)
    if _SUNSET_HOUR <= hour <= 24.0:
        return _mapped_range(_SUNSET_HOUR, 24.0, -180.0, -270.0, hour)
    if 0.0 <= hour <= _SUNRISE_HOUR:
        return _mapped_range(0.0, _SUNRISE_HOUR, -270.0, -360.0, hour)
    raise ValueError("hour is outside one day: %r" % hour)


def _unwind_degrees(value: float) -> float:
    while value > 180.0:
        value -= 360.0
    while value < -180.0:
        value += 360.0
    return value


def _component_pitch(lighting_pitch: float) -> float:
    if lighting_pitch < -90.0:
        return -180.0 - lighting_pitch
    if lighting_pitch > 90.0:
        return 180.0 - lighting_pitch
    return lighting_pitch


def _time_cycle_phase(hour: float) -> tuple[float, float, float]:
    dawn_start = _SUNRISE_HOUR - _DAWN_BLEND_HOURS
    dusk_start = _SUNSET_HOUR - _DUSK_BLEND_HOURS
    day_alpha = 0.0
    warm_weight = 0.0
    solar_temperature = _TRANSITION_DIRECTIONAL_TEMPERATURE
    if dawn_start <= hour < _SUNRISE_HOUR:
        phase_alpha = (hour - dawn_start) / _DAWN_BLEND_HOURS
        day_alpha = phase_alpha
        warm_weight = math.sin(phase_alpha * math.pi)
        solar_temperature = _lerp(
            _TRANSITION_DIRECTIONAL_TEMPERATURE,
            _DAY_DIRECTIONAL_TEMPERATURE,
            phase_alpha,
        )
    elif _SUNRISE_HOUR <= hour < dusk_start:
        day_alpha = 1.0
        solar_temperature = _DAY_DIRECTIONAL_TEMPERATURE
    elif dusk_start <= hour < _SUNSET_HOUR:
        phase_alpha = (hour - dusk_start) / _DUSK_BLEND_HOURS
        day_alpha = 1.0 - phase_alpha
        warm_weight = math.sin(phase_alpha * math.pi)
        solar_temperature = _lerp(
            _DAY_DIRECTIONAL_TEMPERATURE,
            _TRANSITION_DIRECTIONAL_TEMPERATURE,
            phase_alpha,
        )
    return day_alpha, warm_weight, solar_temperature


def cf1_profile_for(variant: str, time_code: str) -> dict:
    """Return the fixed per-time CF1 grade and its one variant mutation."""

    if variant not in _CF1_VARIANTS:
        raise ValueError("unknown CF1 variant: " + str(variant))
    try:
        saturation, contrast, gain_rgb = TIME_GRADE_B[time_code]
    except KeyError as exc:
        raise ValueError("unknown CF1 time: " + str(time_code)) from exc
    midtones = (
        (1.15, 1.15, 1.15, 1.0)
        if variant == "B"
        else (1.0, 1.0, 1.0, 1.0)
    )
    return {
        "saturation": saturation,
        "contrast": contrast,
        "gain_rgb": gain_rgb,
        "color_saturation_midtones": midtones,
        "color_correction_shadows_max": 0.09,
        "color_correction_highlights_min": 0.50,
        "tonemapper_sharpen": 0.4,
        "bloom_intensity": 0.30,
        "bloom_threshold": 3.0,
        "window_emissive_scale": 0.75,
    }


def cf1_expected_lighting(time_code: str) -> dict:
    """Reproduce GetCycleSunPitch and TimeCycleSkyLighting::Calculate."""

    try:
        hour_value, minute_value = CF1_TIMES[time_code]
    except KeyError as exc:
        raise ValueError("unknown CF1 time: " + str(time_code)) from exc
    hour = float(hour_value) + float(minute_value) / 60.0
    raw_pitch = _raw_solar_pitch(hour)
    lighting_pitch = -abs(_unwind_degrees(raw_pitch))
    component_pitch = _component_pitch(lighting_pitch)
    day_alpha, warm_weight, solar_temperature = _time_cycle_phase(hour)
    solar_intensity = _SUN_MAX_INTENSITY * day_alpha
    fill_intensity = (
        N1_DIM_NIGHT_LOOK["night_directional_intensity"] * (1.0 - day_alpha)
    )
    directional_intensity = solar_intensity + fill_intensity
    if directional_intensity > 1.0e-4:
        directional_temperature = (
            solar_intensity * solar_temperature
            + fill_intensity
            * N1_DIM_NIGHT_LOOK["night_directional_temperature"]
        ) / directional_intensity
    else:
        directional_temperature = solar_temperature
    sky_intensity = _lerp(
        N1_DIM_NIGHT_LOOK["sky_night_intensity"],
        _SKY_DAY_INTENSITY,
        day_alpha,
    )
    exposure_bias = _lerp(
        N1_DIM_NIGHT_LOOK["night_exposure_bias"],
        _DAY_EXPOSURE_BIAS,
        day_alpha,
    )
    phase_ambient = _lerp_color(
        N1_DIM_NIGHT_LOOK["night_ambient_color"],
        _BASE_SKY_COLOR,
        day_alpha,
    )
    dusk_color_alpha = min(
        1.0,
        max(0.0, warm_weight * _DUSK_AMBIENT_STRENGTH),
    )
    sky_color = _lerp_color(
        phase_ambient,
        _DUSK_AMBIENT_COLOR,
        dusk_color_alpha,
    )
    return {
        "time_code": time_code,
        "hour": hour_value,
        "minute": minute_value,
        "raw_solar_pitch": raw_pitch,
        "lighting_pitch": lighting_pitch,
        "component_pitch": component_pitch,
        "pitch": component_pitch,
        "effective_azimuth": _SUN_AZIMUTH,
        "solar_intensity": solar_intensity,
        "directional_intensity": directional_intensity,
        "directional_temperature": directional_temperature,
        "use_temperature": True,
        "cast_shadows": False,
        "cast_dynamic_shadows": False,
        "day_alpha": day_alpha,
        "warm_weight": warm_weight,
        "sky_light_intensity": sky_intensity,
        "sky_light_color": sky_color,
        "exposure_bias": exposure_bias,
        "sunrise_hour": _SUNRISE_HOUR,
        "sunset_hour": _SUNSET_HOUR,
        "bloom_intensity": 0.30,
        "bloom_threshold": 3.0,
        "tonemapper_sharpen": 0.4,
        "pc_match_lut_intensity": 1.0,
        "window_emissive_scale": 0.75,
    }


def baseline_safety_errors(metrics_by_time: dict) -> list[str]:
    """Return named, fail-closed errors for the four safety A_PRE metrics."""

    if not isinstance(metrics_by_time, dict):
        return ["baseline safety metrics must be an object"]
    errors = []
    expected_times = set(BASELINE_SAFETY_TIMES)
    for time_code in sorted(expected_times - set(metrics_by_time)):
        errors.append("missing time: " + time_code)
    for time_code in sorted(set(metrics_by_time) - expected_times):
        errors.append("unknown time: " + str(time_code))
    for time_code in BASELINE_SAFETY_TIMES:
        metrics = metrics_by_time.get(time_code)
        if not isinstance(metrics, dict):
            if time_code in metrics_by_time:
                errors.append("%s metrics must be an object" % time_code)
            continue
        values = {}
        for field in ("p50", "under_0p02_percent", "over_0p8_percent"):
            value = metrics.get(field)
            if (
                isinstance(value, bool)
                or not isinstance(value, (int, float))
                or not math.isfinite(float(value))
            ):
                errors.append("%s %s must be finite" % (time_code, field))
            else:
                values[field] = float(value)
        limits = BASELINE_SAFETY_LIMITS[time_code]
        p50 = values.get("p50")
        if p50 is not None and not (limits["p50"][0] <= p50 <= limits["p50"][1]):
            errors.append(
                "%s p50 outside [%s, %s]"
                % (time_code, limits["p50"][0], limits["p50"][1])
            )
        under = values.get("under_0p02_percent")
        if under is not None and under > limits["under_0p02_max"]:
            errors.append(
                "%s under_0p02_percent exceeds %s"
                % (time_code, limits["under_0p02_max"])
            )
        over = values.get("over_0p8_percent")
        if over is not None and over > limits["over_0p8_max"]:
            errors.append(
                "%s over_0p8_percent exceeds %s"
                % (time_code, limits["over_0p8_max"])
            )
    return errors


def expected_gate_policy() -> dict:
    """Return the exact preview policy payload as a detached JSON object."""

    return {
        "schema_version": 1,
        "authority": AUTHORITY_MODE,
        "policy_id": "CF1_BASELINE_SAFETY_V1",
        "algorithm": "linear_rec709_v1",
        "png_decode": "RGBA_to_linear_RGB",
        "resolution": [1600, 900],
        "safety_roi": list(FULL_FRAME_ROI),
        "building_roi": list(BUILDING_ROI),
        "limits": {
            time_code: {
                "p50": list(limits["p50"]),
                "under_0p02_max": limits["under_0p02_max"],
                "over_0p8_max": limits["over_0p8_max"],
            }
            for time_code, limits in BASELINE_SAFETY_LIMITS.items()
        },
        "a_a_mean_abs_rgb_max": A_A_MEAN_ABS_RGB_MAX,
    }


def _reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate policy key: " + str(key))
        result[key] = value
    return result


def _reject_nonfinite_json(value):
    raise ValueError("non-finite JSON value: " + str(value))


def _canonical_json_bytes(value) -> bytes:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")


def gate_policy_sha256(policy_path=None) -> str:
    """Validate the physical policy against code, then hash canonical JSON."""

    path = pathlib.Path(policy_path) if policy_path is not None else _POLICY_PATH
    try:
        payload = json.loads(
            path.read_text(encoding="utf-8"),
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_nonfinite_json,
        )
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        raise ValueError("invalid CF1 baseline safety policy: %s" % exc) from exc
    expected = expected_gate_policy()
    if not isinstance(payload, dict):
        raise ValueError("CF1 baseline safety policy must be an object")
    missing = sorted(set(expected) - set(payload))
    unknown = sorted(set(payload) - set(expected))
    if missing:
        raise ValueError("CF1 baseline safety policy missing keys: %s" % missing)
    if unknown:
        raise ValueError("CF1 baseline safety policy unknown keys: %s" % unknown)
    if payload != expected:
        raise ValueError("CF1 baseline safety policy differs from code constants")
    payload_canonical = _canonical_json_bytes(payload)
    expected_canonical = _canonical_json_bytes(expected)
    if payload_canonical != expected_canonical:
        raise ValueError(
            "CF1 baseline safety policy canonical JSON differs from code constants"
        )
    return hashlib.sha256(expected_canonical).hexdigest()


def _normalize_json(value):
    if value is None or isinstance(value, (str, bool, int)):
        return value
    if isinstance(value, float):
        if not math.isfinite(value):
            raise ValueError("mutation contract numbers must be finite")
        if value == 0.0:
            return 0
        if value.is_integer():
            return int(value)
        return value
    if isinstance(value, list):
        return [_normalize_json(item) for item in value]
    if isinstance(value, dict):
        if any(not isinstance(key, str) for key in value):
            raise ValueError("mutation contract keys must be strings")
        return {key: _normalize_json(item) for key, item in value.items()}
    raise ValueError("mutation contract contains a non-JSON value")


def cf1_mutation_contract() -> dict:
    """Return the approved literal contract without claiming root authority."""

    return copy.deepcopy(_MUTATION_CONTRACT)


def cf1_mutation_signature() -> str:
    normalized = _normalize_json(cf1_mutation_contract())
    return "sha256:" + hashlib.sha256(
        _canonical_json_bytes(normalized)
    ).hexdigest()


def cf1_saved_dir_suffix(stage: str, run_id: str) -> str:
    prefixes = {
        "BASELINE_SAFETY": "CF1BaselineSafety",
        "AB": "CF1AB",
    }
    try:
        prefix = prefixes[stage]
    except (KeyError, TypeError) as exc:
        raise ValueError("unknown CF1 stage: " + str(stage)) from exc
    if not isinstance(run_id, str):
        raise ValueError("CF1 run ID must be a canonical lowercase UUID")
    try:
        parsed = uuid.UUID(run_id)
    except (ValueError, AttributeError) as exc:
        raise ValueError("CF1 run ID must be a canonical lowercase UUID") from exc
    if str(parsed) != run_id:
        raise ValueError("CF1 run ID must be a canonical lowercase UUID")
    return prefix + parsed.hex
