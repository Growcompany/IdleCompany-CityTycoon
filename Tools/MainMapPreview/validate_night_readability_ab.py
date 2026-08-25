# -*- coding: utf-8 -*-
"""Validate and measure the MainMap Vulkan ES3.1 night-readability A/B."""

import json
import os
import sys

import numpy as np
from PIL import Image, ImageChops, ImageStat


MARKER = "[CGR-MAINMAP-NIGHT-AB-VULKAN] "
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUTPUT_DIR = os.path.join(
    ROOT,
    "Saved",
    "BuildingLookdev",
    "MainMapPreview",
    "NightReadabilityAB",
    "VulkanES31",
)
LOG_PATH = os.path.join(
    ROOT,
    "Saved",
    "BuildingLookdev",
    "Logs",
    "MainMapNightReadabilityAB_VulkanES31.log",
)
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
TIMES = ("1930", "0000", "0530")
CAPTURES = {
    "%s_%s" % (variant, time_code): os.path.join(
        OUTPUT_DIR,
        "MainMap_%s_%s.png" % (variant, time_code),
    )
    for variant in VARIANTS
    for time_code in TIMES
}


def _last_result():
    if not os.path.exists(LOG_PATH):
        raise AssertionError("capture log is missing: " + LOG_PATH)
    result = None
    with open(LOG_PATH, "r", encoding="utf-8", errors="replace") as stream:
        log_text = stream.read()
        for line in log_text.splitlines():
            if MARKER in line and "{" in line:
                result = json.loads(line.split(MARKER, 1)[1].strip())
    if result is None:
        raise AssertionError("capture result marker is missing")
    return result, log_text


def _nearly_equal(actual, expected, tolerance=0.001):
    return actual is not None and abs(float(actual) - expected) <= tolerance


def _linear_luminance_metrics(image):
    rgb = np.asarray(image.convert("RGB"), dtype=np.float32) / 255.0
    linear = np.where(
        rgb <= 0.04045,
        rgb / 12.92,
        ((rgb + 0.055) / 1.055) ** 2.4,
    )
    luminance = (
        linear[..., 0] * 0.2126
        + linear[..., 1] * 0.7152
        + linear[..., 2] * 0.0722
    )
    return {
        "p50": round(float(np.percentile(luminance, 50)), 5),
        "under_0p02_percent": round(float(np.mean(luminance < 0.02) * 100.0), 2),
        "over_0p8_percent": round(float(np.mean(luminance > 0.8) * 100.0), 2),
    }


def _mean_rgb_difference(left, right):
    difference = ImageChops.difference(left.convert("RGB"), right.convert("RGB"))
    return round(sum(ImageStat.Stat(difference).mean) / 3.0, 4)


def _validate_cycle_duration(value, label, errors):
    expected = {"hours": 0, "minutes": 0, "seconds": 24}
    if value != expected:
        errors.append("%s cycle duration changed: %r" % (label, value))


def run():
    errors = []
    try:
        result, log_text = _last_result()
    except Exception as exc:
        result = None
        log_text = ""
        errors.append(str(exc))

    if result is not None:
        if not result.get("ok"):
            errors.append("capture reported failure: %s" % result.get("error", ""))

        command_line = result.get("command_line", "").lower()
        if "-vulkan" not in command_line:
            errors.append("capture command line is missing -vulkan")
        if "-featureleveles31" not in command_line:
            errors.append("capture command line is missing -FeatureLevelES31")
        if "Using Forced RHI: Vulkan" not in log_text:
            errors.append("capture log does not prove the Vulkan RHI was selected")
        if "Using Forced Feature Level in Editor: ES3_1" not in log_text:
            errors.append("capture log does not prove ES3_1 was selected")

        cvars = result.get("cvars", {})
        if cvars.get("r.Mobile.ShadingPath") != 0:
            errors.append("capture did not use Mobile Forward")
        if cvars.get("r.MobileHDR") != 1:
            errors.append("capture did not use Mobile HDR")
        if cvars.get("r.EyeAdaptation.MethodOverride") != 2:
            errors.append("capture did not use Manual eye adaptation")
        if result.get("building_keys") != 40:
            errors.append("expected 40 mapped MainMap buildings")
        if result.get("window_emissive_mids") != 200:
            errors.append("expected all 200 facade window MIDs")
        if result.get("sunset_hour") != 19:
            errors.append("transient A/B sunset must be 19:00")
        if not _nearly_equal(result.get("night_directional_temperature"), 9000.0):
            errors.append("night directional temperature must be 9000 K")

        _validate_cycle_duration(result.get("cycle_duration_before"), "before", errors)
        _validate_cycle_duration(result.get("cycle_duration_after"), "after", errors)

        expected_sunset_before = {"hours": 17, "minutes": 0, "seconds": 0}
        expected_sunset_ab = {"hours": 19, "minutes": 0, "seconds": 0}
        if result.get("sunset_before") != expected_sunset_before:
            errors.append("placed sunset before A/B must be 17:00")
        if result.get("sunset_applied") != expected_sunset_ab:
            errors.append("transient sunset was not applied as 19:00")
        if result.get("sunset_after") != expected_sunset_ab:
            errors.append("transient sunset changed during capture")

        directional = result.get("directional_light") or {}
        if directional.get("sky_count") != 1:
            errors.append("TimeCycleSky must use exactly one DirectionalLight")
        if directional.get("world_count") != 1:
            errors.append("MainMap must contain exactly one DirectionalLight")
        if directional.get("cast_shadows") is not False:
            errors.append("mobile TimeCycleSky DirectionalLight must not cast shadows")

        atmosphere_expected = {
            "1200": True,
            "0655": False,
            "0705": True,
            "1855": True,
            "1905": False,
        }
        atmosphere_readbacks = result.get("atmosphere_readbacks") or {}
        for label, expected in atmosphere_expected.items():
            readback = atmosphere_readbacks.get(label) or {}
            if readback.get("cast_shadows") is not False:
                errors.append("DirectionalLight shadows enabled at " + label)
            if readback.get("atmosphere_sun_light") is not expected:
                errors.append("Atmosphere Sun Light mismatch at " + label)

        capture_light_readbacks = result.get("capture_light_readbacks") or {}
        for label in CAPTURES:
            readback = capture_light_readbacks.get(label) or {}
            if readback.get("cast_shadows") is not False:
                errors.append("DirectionalLight shadows enabled at " + label)
            if readback.get("atmosphere_sun_light") is not False:
                errors.append("folded night fill drove Atmosphere at " + label)

        day_values = result.get("preserved_day_values", {})
        if not _nearly_equal(day_values.get("sun_max_intensity"), 0.8):
            errors.append("placed SunMaxIntensity must remain 0.8")
        if not _nearly_equal(day_values.get("sky_day_intensity"), 0.5):
            errors.append("placed SkyDayIntensity must remain 0.5")
        if not _nearly_equal(day_values.get("day_exposure_bias"), -0.15):
            errors.append("placed DayExposureBias must remain -0.15")

        applied_variants = result.get("variants", {})
        for variant, expected_values in VARIANTS.items():
            actual_values = applied_variants.get(variant, {})
            for name, expected in expected_values.items():
                if not _nearly_equal(actual_values.get(name), expected):
                    errors.append(
                        "%s %s mismatch: %r" % (
                            variant,
                            name,
                            actual_values.get(name),
                        )
                    )

        actual_readbacks = result.get("applied_variants", {})
        for variant, expected_values in VARIANTS.items():
            readback = actual_readbacks.get(variant, {})
            for name, expected in expected_values.items():
                if not _nearly_equal(readback.get(name), expected):
                    errors.append(
                        "%s applied %s mismatch: %r" % (
                            variant,
                            name,
                            readback.get(name),
                        )
                    )
            if not _nearly_equal(
                readback.get("night_directional_temperature"), 9000.0
            ):
                errors.append(variant + " applied temperature mismatch")
            ambient = readback.get("night_ambient_color") or []
            if len(ambient) != 4 or any(
                not _nearly_equal(actual, expected)
                for actual, expected in zip(
                    ambient, (0.3763, 0.4564, 0.5776, 1.0)
                )
            ):
                errors.append(variant + " applied ambient color mismatch")
            if readback.get("window_emissive_mids_verified") != 200:
                errors.append(variant + " did not verify all 200 window MIDs")
            if not _nearly_equal(
                readback.get("window_emissive_max_abs_error"), 0.0
            ):
                errors.append(variant + " window emissive readback mismatch")

        captured_files = result.get("captures", {})
        for label in CAPTURES:
            capture = captured_files.get(label, {})
            if not capture.get("exists") or int(capture.get("bytes", 0)) <= 0:
                errors.append("capture marker reports missing file: " + label)

    images = {}
    for label, path in CAPTURES.items():
        if not os.path.exists(path):
            errors.append("missing capture: " + path)
            continue
        image = Image.open(path).convert("RGB")
        if image.size != (1600, 900):
            errors.append("unexpected capture size for %s: %r" % (label, image.size))
        images[label] = image

    metrics = {}
    for label, image in images.items():
        metrics[label] = _linear_luminance_metrics(image)

    differences = {}
    for time_code in TIMES:
        n1_label = "N1_" + time_code
        n2_label = "N2_" + time_code
        if n1_label not in images or n2_label not in images:
            continue
        difference = _mean_rgb_difference(images[n1_label], images[n2_label])
        differences[time_code] = difference
        if difference < 0.5:
            errors.append("N1/N2 are visually identical at " + time_code)

    if "N1_0000" in metrics and "N2_0000" in metrics:
        if metrics["N2_0000"]["p50"] <= metrics["N1_0000"]["p50"]:
            errors.append("N2 midnight median luminance must exceed N1")

    report = {
        "ok": not errors,
        "errors": errors,
        "metrics": metrics,
        "n1_n2_mean_rgb_difference": differences,
        "heuristic_targets": {
            "p50": [0.045, 0.065],
            "under_0p02_percent_max": 35.0,
            "over_0p8_percent_max": 0.5,
        },
    }
    print("[CGR-MAINMAP-NIGHT-AB-VALIDATE] " + json.dumps(report, ensure_ascii=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(run())
