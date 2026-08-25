import hashlib
import json
import math
import pathlib
import tempfile
import unittest

import mobile_cinematic_filter as cf1


EXPECTED_MUTATION_CONTRACT = {
    "property": "PostProcessVolume.Settings.ColorSaturationMidtones",
    "baseline": [1, 1, 1, 1],
    "candidate": [1.15, 1.15, 1.15, 1],
    "time_set": ["0900", "1800", "1930", "0000", "0530"],
    "renderer": "Vulkan",
    "feature_level": "ES3_1",
    "map": "MainMap_TheRiverwalkCity",
}
EXPECTED_MUTATION_SIGNATURE = (
    "sha256:023bfdb101eb76f61041a14e95b3b5ab"
    "446ae80afad0acce500aea843e5df8ef"
)


class MobileCinematicFilterTests(unittest.TestCase):
    def test_cf1_constants_match_the_approved_preview_contract(self):
        self.assertEqual(cf1.AUTHORITY_MODE, "preview_non_authoritative")
        self.assertEqual(
            cf1.CF1_TIMES,
            {
                "0900": (9, 0),
                "1800": (18, 0),
                "1930": (19, 30),
                "0000": (0, 0),
                "0530": (5, 30),
            },
        )
        self.assertEqual(
            cf1.BASELINE_SAFETY_TIMES,
            ("1800", "1930", "0000", "0530"),
        )
        self.assertEqual(
            cf1.N1_DIM_NIGHT_LOOK,
            {
                "sky_night_intensity": 0.20,
                "night_directional_intensity": 0.08,
                "night_directional_temperature": 9000.0,
                "night_ambient_color": (0.3763, 0.4564, 0.5776, 1.0),
                "night_exposure_bias": 0.20,
            },
        )
        self.assertEqual(cf1.FULL_FRAME_ROI, (0, 0, 1600, 900))
        self.assertEqual(cf1.BUILDING_ROI, (160, 25, 1400, 760))
        self.assertEqual(cf1.A_A_MEAN_ABS_RGB_MAX, 0.001)
        self.assertEqual(
            cf1.L2_LUT_SOURCE_SHA256,
            "b9d21a63b85f5f2da146d68d4b51e590"
            "eadcf693f347c5693cea391ca9a46dff",
        )

    def test_cf1_b_changes_only_midtone_saturation(self):
        control = cf1.cf1_profile_for("A_PRE", "1930")
        candidate = cf1.cf1_profile_for("B", "1930")

        self.assertEqual(
            control["color_saturation_midtones"],
            (1.0, 1.0, 1.0, 1.0),
        )
        self.assertEqual(
            candidate["color_saturation_midtones"],
            (1.15, 1.15, 1.15, 1.0),
        )
        self.assertEqual(
            {key for key in control if control[key] != candidate[key]},
            {"color_saturation_midtones"},
        )

    def test_cf1_a_pre_and_a_post_are_identical_at_every_time(self):
        for time_code in cf1.CF1_TIMES:
            with self.subTest(time_code=time_code):
                self.assertEqual(
                    cf1.cf1_profile_for("A_PRE", time_code),
                    cf1.cf1_profile_for("A_POST", time_code),
                )

    def test_cf1_boundaries_and_common_render_values_are_fixed(self):
        for time_code in cf1.CF1_TIMES:
            for variant in ("A_PRE", "B", "A_POST"):
                with self.subTest(time_code=time_code, variant=variant):
                    profile = cf1.cf1_profile_for(variant, time_code)
                    self.assertEqual(
                        profile["color_correction_shadows_max"], 0.09
                    )
                    self.assertEqual(
                        profile["color_correction_highlights_min"], 0.50
                    )
                    self.assertNotIn("pc_match_lut_intensity", profile)
                    self.assertNotIn("wall_color_scale", profile)
                    self.assertEqual(profile["tonemapper_sharpen"], 0.4)
                    self.assertEqual(profile["bloom_intensity"], 0.30)
                    self.assertEqual(profile["bloom_threshold"], 3.0)
                    self.assertEqual(profile["window_emissive_scale"], 0.75)

    def test_cf1_time_grade_b_is_used_for_every_variant(self):
        for time_code, expected in cf1.TIME_GRADE_B.items():
            for variant in ("A_PRE", "B", "A_POST"):
                with self.subTest(time_code=time_code, variant=variant):
                    profile = cf1.cf1_profile_for(variant, time_code)
                    self.assertEqual(profile["saturation"], expected[0])
                    self.assertEqual(profile["contrast"], expected[1])
                    self.assertEqual(profile["gain_rgb"], expected[2])

    def test_cf1_profile_rejects_unknown_time_or_variant(self):
        with self.assertRaisesRegex(ValueError, "unknown CF1 variant"):
            cf1.cf1_profile_for("A", "0900")
        with self.assertRaisesRegex(ValueError, "unknown CF1 time"):
            cf1.cf1_profile_for("B", "1200")

    def test_cf1_mutation_contract_and_signature_are_canonical_literals(self):
        self.assertEqual(cf1.cf1_mutation_contract(), EXPECTED_MUTATION_CONTRACT)
        self.assertEqual(
            cf1.cf1_mutation_signature(), EXPECTED_MUTATION_SIGNATURE
        )
        canonical = json.dumps(
            EXPECTED_MUTATION_CONTRACT,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        self.assertEqual(
            cf1.cf1_mutation_signature(),
            "sha256:" + hashlib.sha256(canonical).hexdigest(),
        )

    def test_cf1_mutation_contract_returns_a_detached_copy(self):
        changed = cf1.cf1_mutation_contract()
        changed["candidate"][0] = 99
        self.assertEqual(cf1.cf1_mutation_contract(), EXPECTED_MUTATION_CONTRACT)

    def test_cf1_saved_dir_suffix_is_run_scoped_and_parser_safe(self):
        run_id = "12345678-1234-1234-1234-1234567890ab"
        self.assertEqual(
            cf1.cf1_saved_dir_suffix("BASELINE_SAFETY", run_id),
            "CF1BaselineSafety123456781234123412341234567890ab",
        )
        self.assertEqual(
            cf1.cf1_saved_dir_suffix("AB", run_id),
            "CF1AB123456781234123412341234567890ab",
        )
        for stage, candidate in (
            ("UNKNOWN", run_id),
            ("AB", run_id.upper()),
            ("AB", "123456781234123412341234567890ab"),
            ("AB", run_id + " -log"),
        ):
            with self.subTest(stage=stage, candidate=candidate):
                with self.assertRaises(ValueError):
                    cf1.cf1_saved_dir_suffix(stage, candidate)

    def test_cf1_expected_lighting_matches_time_cycle_math(self):
        expected = {
            "0900": (-36.0, 0.8, 6500.0, 0.5, -0.15),
            "1800": (-12.8571428571, 0.44, 4590.9090909, 0.35, 0.025),
            "1930": (-9.0, 0.08, 9000.0, 0.20, 0.20),
            "0000": (-90.0, 0.08, 9000.0, 0.20, 0.20),
            "0530": (-19.2857142857, 0.26, 4365.3846154, 0.275, 0.1125),
        }

        for code, values in expected.items():
            with self.subTest(code=code):
                state = cf1.cf1_expected_lighting(code)
                self.assertAlmostEqual(state["pitch"], values[0], places=4)
                self.assertAlmostEqual(
                    state["directional_intensity"], values[1], places=4
                )
                self.assertAlmostEqual(
                    state["directional_temperature"], values[2], places=3
                )
                self.assertAlmostEqual(
                    state["sky_light_intensity"], values[3], places=4
                )
                self.assertAlmostEqual(
                    state["exposure_bias"], values[4], places=4
                )

    def test_cf1_sky_color_matches_mainmap_placed_dusk_settings(self):
        expected = {
            "0900": (
                0.665387295591707,
                0.665387295591707,
                0.665387295591707,
                1.0,
            ),
            "1800": (
                0.7125061886775121,
                0.5845361886775121,
                0.5248961886775121,
                1.0,
            ),
            "1930": (0.3763, 0.4564, 0.5776, 1.0),
            "0000": (0.3763, 0.4564, 0.5776, 1.0),
            "0530": (
                0.6045392649615691,
                0.5401422582693011,
                0.5374496047114495,
                1.0,
            ),
        }

        for time_code, sky_color in expected.items():
            with self.subTest(time_code=time_code):
                self.assertEqual(
                    cf1.cf1_expected_lighting(time_code)["sky_light_color"],
                    sky_color,
                )

    def test_cf1_raw_lighting_and_effective_azimuth_are_explicit(self):
        at_1800 = cf1.cf1_expected_lighting("1800")
        self.assertAlmostEqual(
            at_1800["raw_solar_pitch"], -167.1428571429, places=6
        )
        self.assertAlmostEqual(
            at_1800["lighting_pitch"], -167.1428571429, places=6
        )
        self.assertAlmostEqual(
            at_1800["component_pitch"], -12.8571428571, places=6
        )
        at_1930 = cf1.cf1_expected_lighting("1930")
        self.assertEqual(at_1930["raw_solar_pitch"], -189.0)
        self.assertEqual(at_1930["lighting_pitch"], -171.0)
        self.assertEqual(at_1930["component_pitch"], -9.0)
        self.assertEqual(at_1930["effective_azimuth"], 35.0)

    def test_cf1_lighting_includes_the_fixed_mobile_readback_contract(self):
        for time_code in cf1.CF1_TIMES:
            with self.subTest(time_code=time_code):
                state = cf1.cf1_expected_lighting(time_code)
                self.assertEqual(state["sunrise_hour"], 7.0)
                self.assertEqual(state["sunset_hour"], 19.0)
                self.assertTrue(state["use_temperature"])
                self.assertFalse(state["cast_shadows"])
                self.assertFalse(state["cast_dynamic_shadows"])
                self.assertEqual(state["effective_azimuth"], 35.0)
                self.assertEqual(state["bloom_intensity"], 0.30)
                self.assertEqual(state["bloom_threshold"], 3.0)
                self.assertEqual(state["tonemapper_sharpen"], 0.4)
                self.assertEqual(state["pc_match_lut_intensity"], 1.0)
                self.assertEqual(state["window_emissive_scale"], 0.75)
                self.assertEqual(len(state["sky_light_color"]), 4)
                self.assertTrue(
                    all(math.isfinite(value) for value in state["sky_light_color"])
                )

    def test_baseline_safety_limits_are_inclusive(self):
        metrics = {
            time_code: {
                "p50": limits["p50"][0],
                "under_0p02_percent": limits["under_0p02_max"],
                "over_0p8_percent": limits["over_0p8_max"],
            }
            for time_code, limits in cf1.BASELINE_SAFETY_LIMITS.items()
        }
        self.assertEqual(cf1.baseline_safety_errors(metrics), [])
        for time_code, limits in cf1.BASELINE_SAFETY_LIMITS.items():
            metrics[time_code]["p50"] = limits["p50"][1]
        self.assertEqual(cf1.baseline_safety_errors(metrics), [])

    def test_baseline_safety_rejects_outside_and_malformed_metrics(self):
        passing = {
            time_code: {
                "p50": sum(limits["p50"]) / 2.0,
                "under_0p02_percent": limits["under_0p02_max"],
                "over_0p8_percent": limits["over_0p8_max"],
            }
            for time_code, limits in cf1.BASELINE_SAFETY_LIMITS.items()
        }
        changed = json.loads(json.dumps(passing))
        changed["1930"]["p50"] = 0.035 - 0.00001
        self.assertIn("1930 p50", "\n".join(cf1.baseline_safety_errors(changed)))
        changed = json.loads(json.dumps(passing))
        changed["0000"]["under_0p02_percent"] = 40.00001
        self.assertIn(
            "0000 under_0p02_percent",
            "\n".join(cf1.baseline_safety_errors(changed)),
        )
        changed = json.loads(json.dumps(passing))
        changed["0530"]["over_0p8_percent"] = float("nan")
        self.assertIn(
            "0530 over_0p8_percent",
            "\n".join(cf1.baseline_safety_errors(changed)),
        )
        changed = json.loads(json.dumps(passing))
        del changed["1800"]
        changed["1200"] = passing["1800"]
        errors = "\n".join(cf1.baseline_safety_errors(changed))
        self.assertIn("missing time: 1800", errors)
        self.assertIn("unknown time: 1200", errors)

    def test_policy_file_matches_code_and_has_a_canonical_hash(self):
        policy_path = pathlib.Path(cf1.__file__).with_name(
            "cf1_baseline_safety_policy.json"
        )
        policy = json.loads(policy_path.read_text(encoding="utf-8"))
        self.assertEqual(policy, cf1.expected_gate_policy())
        self.assertEqual(policy["authority"], "preview_non_authoritative")
        canonical = json.dumps(
            policy,
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        self.assertEqual(
            cf1.gate_policy_sha256(), hashlib.sha256(canonical).hexdigest()
        )

    def test_policy_loader_fails_closed_on_missing_unknown_or_changed_values(self):
        policy = cf1.expected_gate_policy()
        mutations = []
        missing = dict(policy)
        missing.pop("algorithm")
        mutations.append(missing)
        unknown = dict(policy, unexpected=True)
        mutations.append(unknown)
        changed = dict(policy, resolution=[1280, 720])
        mutations.append(changed)

        for index, payload in enumerate(mutations):
            with self.subTest(index=index):
                with tempfile.TemporaryDirectory() as directory:
                    path = pathlib.Path(directory) / "policy.json"
                    path.write_text(json.dumps(payload), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        cf1.gate_policy_sha256(path)

    def test_policy_loader_rejects_integral_float_representation_drift(self):
        drifted = cf1.expected_gate_policy()
        drifted["limits"]["1800"]["under_0p02_max"] = 20
        self.assertEqual(drifted, cf1.expected_gate_policy())

        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "policy.json"
            path.write_text(json.dumps(drifted), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "canonical JSON"):
                cf1.gate_policy_sha256(path)


if __name__ == "__main__":
    unittest.main()
