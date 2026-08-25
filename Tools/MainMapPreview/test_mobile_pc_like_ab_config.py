import unittest

import mobile_pc_like_ab_config as config
from mobile_pc_like_ab_config import (
    adopted_from_command_line,
    capture_sequence,
    ensure_capture_supported,
    experiment_from_command_line,
    launch_contract_from_command_line,
    profile_for,
    saved_suffix_for,
)


def _required_api(name):
    value = getattr(config, name, None)
    if value is None:
        raise AssertionError("missing required API: " + name)
    return value


class MobilePcLikeAbConfigTests(unittest.TestCase):
    def test_s1_loads_only_the_two_transient_semantic_skin_masters(self):
        load_for_experiment = _required_api(
            "load_semantic_skin_masters_for_experiment"
        )
        calls = []

        loaded = load_for_experiment(
            "S1", lambda path: calls.append(path) or path
        )

        self.assertEqual(
            loaded,
            {
                "Complex": (
                    "/Game/__CGRTransient/MobileVisualAB/SemanticSkin/"
                    "M_MB_Complex_S1.M_MB_Complex_S1"
                ),
                "Simple": (
                    "/Game/__CGRTransient/MobileVisualAB/SemanticSkin/"
                    "M_MB_Simple_S1.M_MB_Simple_S1"
                ),
            },
        )
        self.assertEqual(calls, [loaded["Complex"], loaded["Simple"]])
        self.assertIsNone(
            load_for_experiment("L1", lambda path: self.fail(path))
        )

    def test_s1_changes_only_facade_look_transfer_strength(self):
        control = profile_for("S1", "A_PRE", ("E2", "L1"))
        candidate = profile_for("S1", "B", ("E2", "L1"))

        self.assertEqual(control["facade_look_transfer_strength"], 0.0)
        self.assertEqual(candidate["facade_look_transfer_strength"], 1.0)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"facade_look_transfer_strength"},
        )

    def test_s2_changes_only_luma_locked_facade_chroma_strength(self):
        control = profile_for("S2", "A_PRE", ("E2", "L1"))
        candidate = profile_for("S2", "B", ("E2", "L1"))

        self.assertEqual(control["facade_look_transfer_strength"], 0.0)
        self.assertEqual(candidate["facade_look_transfer_strength"], 1.0)
        self.assertEqual(control["pc_match_lut_intensity"], 0.35)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"facade_look_transfer_strength"},
        )

    def test_s2_loads_same_transient_masters_and_requires_l1(self):
        load_semantic = _required_api(
            "load_semantic_skin_masters_for_experiment"
        )
        load_lut = _required_api("load_pc_match_lut_for_experiment")

        semantic = load_semantic("S2", lambda path: path)
        self.assertEqual(semantic, config.SEMANTIC_SKIN_MASTER_OBJECT_PATHS)
        self.assertEqual(
            load_lut("S2", lambda path: path, adopted=("E2", "L1")),
            config.PC_MATCH_LUT_OBJECT_PATH,
        )
        with self.assertRaisesRegex(
            ValueError, "S2 requires exact adopted baseline E2,L1"
        ):
            profile_for("S2", "B", ("E2",))
        with self.assertRaisesRegex(
            ValueError, "S2 requires exact adopted baseline E2,L1"
        ):
            load_lut("S2", lambda path: path, adopted=("E2",))

    def test_s2_launch_contract_is_unique_and_ordered_after_s1(self):
        self.assertEqual(
            launch_contract_from_command_line(
                "-CGRProbe=MobilePCLikeAB -CGRExperiment=S2 "
                "-CGRAdopted=E2,L1 "
                "-saveddirsuffix=MainMapMobilePCLikeS2AB"
            ),
            ("S2", ("E2", "L1")),
        )
        self.assertEqual(saved_suffix_for("S2"), "MainMapMobilePCLikeS2AB")
        self.assertEqual(config.ORDER.index("S2"), config.ORDER.index("S1") + 1)

    def test_s2_rejects_missing_e2_or_extra_s1_from_its_exact_baseline(self):
        for adopted in (("L1",), ("E2", "L1", "S1")):
            with self.subTest(adopted=adopted):
                with self.assertRaisesRegex(
                    ValueError, "S2 requires exact adopted baseline E2,L1"
                ):
                    profile_for("S2", "A_PRE", adopted)

    def test_s3_changes_only_source_pbr_bake_on_exact_e2_l1_baseline(self):
        control = profile_for("S3", "A_PRE", ("E2", "L1"))
        candidate = profile_for("S3", "B", ("E2", "L1"))

        self.assertFalse(control["source_pbr_bake"])
        self.assertTrue(candidate["source_pbr_bake"])
        self.assertEqual(control["wall_color_scale"], 0.92)
        self.assertEqual(control["pc_match_lut_intensity"], 0.35)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"source_pbr_bake"},
        )

    def test_s3_uses_lut_without_semantic_material_swap(self):
        load_lut = _required_api("load_pc_match_lut_for_experiment")
        load_semantic = _required_api(
            "load_semantic_skin_masters_for_experiment"
        )

        self.assertEqual(
            load_lut("S3", lambda path: path, adopted=("E2", "L1")),
            config.PC_MATCH_LUT_OBJECT_PATH,
        )
        self.assertIsNone(load_semantic("S3", lambda path: self.fail(path)))
        self.assertEqual(config.ORDER.index("S3"), config.ORDER.index("S2") + 1)
        self.assertEqual(ensure_capture_supported("S3"), "S3")

    def test_s3_rejects_missing_or_extra_adopted_baseline(self):
        for adopted in (("L1",), ("E2",), ("E2", "L1", "S1")):
            with self.subTest(adopted=adopted):
                with self.assertRaisesRegex(
                    ValueError, "S3 requires exact adopted baseline E2,L1"
                ):
                    profile_for("S3", "A_PRE", adopted)

        self.assertEqual(
            launch_contract_from_command_line(
                "-CGRProbe=MobilePCLikeAB -CGRExperiment=S3 "
                "-CGRAdopted=E2,L1 -saveddirsuffix=MainMapMobilePCLikeS3AB"
            ),
            ("S3", ("E2", "L1")),
        )

    def test_l2_changes_only_existing_pc_match_lut_to_full_strength(self):
        control = profile_for("L2", "A_PRE", ("E2", "L1"))
        candidate = profile_for("L2", "B", ("E2", "L1"))

        self.assertEqual(control["pc_match_lut_intensity"], 0.35)
        self.assertEqual(candidate["pc_match_lut_intensity"], 1.0)
        self.assertEqual(control["wall_color_scale"], 0.92)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"pc_match_lut_intensity"},
        )
        self.assertEqual(
            config.load_pc_match_lut_for_experiment(
                "L2", lambda path: path, adopted=("E2", "L1")
            ),
            config.PC_MATCH_LUT_OBJECT_PATH,
        )
        self.assertEqual(ensure_capture_supported("L2"), "L2")
        self.assertEqual(config.ORDER.index("L2"), config.ORDER.index("R1") + 1)

    def test_l2_requires_exact_e2_l1_baseline(self):
        for adopted in (("L1",), ("E2",), ("E2", "L1", "S1")):
            with self.subTest(adopted=adopted):
                with self.assertRaisesRegex(
                    ValueError, "L2 requires exact adopted baseline E2,L1"
                ):
                    profile_for("L2", "A_PRE", adopted)

        self.assertEqual(
            launch_contract_from_command_line(
                "-CGRProbe=MobilePCLikeAB -CGRExperiment=L2 "
                "-CGRAdopted=E2,L1 -saveddirsuffix=MainMapMobilePCLikeL2AB"
            ),
            ("L2", ("E2", "L1")),
        )

    def test_r1_changes_only_baked_reflection_mode_on_exact_e2_l1_baseline(self):
        control = profile_for("R1", "A_PRE", ("E2", "L1"))
        candidate = profile_for("R1", "B", ("E2", "L1"))

        self.assertEqual(control["reflection_mode"], "broad_emissive_add")
        self.assertEqual(candidate["reflection_mode"], "baked_energy_lerp")
        self.assertEqual(
            {key for key in control if control[key] != candidate[key]},
            {"reflection_mode"},
        )
        self.assertEqual(
            _required_api("load_pc_match_lut_for_experiment")(
                "R1", lambda path: path, adopted=("E2", "L1")
            ),
            config.PC_MATCH_LUT_OBJECT_PATH,
        )
        self.assertEqual(ensure_capture_supported("R1"), "R1")
        self.assertEqual(config.ORDER.index("R1"), config.ORDER.index("S3") + 1)

        for adopted in (("E2",), ("L1",), ("E2", "L1", "S1")):
            with self.subTest(adopted=adopted):
                with self.assertRaisesRegex(
                    ValueError, "R1 requires exact adopted baseline E2,L1"
                ):
                    profile_for("R1", "A_PRE", adopted)

    def test_l1_loads_transient_lut_at_bootstrap_before_warmup(self):
        load_for_experiment = _required_api("load_pc_match_lut_for_experiment")
        calls = []

        loaded = load_for_experiment("L1", lambda path: calls.append(path) or object())

        self.assertIsNotNone(loaded)
        self.assertEqual(
            calls,
            [
                "/Game/__CGRTransient/MobileVisualAB/"
                "T_PCMatch_Day_LUT.T_PCMatch_Day_LUT"
            ],
        )
        self.assertIsNone(
            load_for_experiment("E4", lambda path: self.fail(path))
        )

    def test_s1_reloads_the_provisionally_adopted_l1_lut(self):
        load_for_experiment = _required_api("load_pc_match_lut_for_experiment")
        calls = []

        loaded = load_for_experiment(
            "S1",
            lambda path: calls.append(path) or path,
            adopted=("E2", "L1"),
        )

        self.assertEqual(loaded, config.PC_MATCH_LUT_OBJECT_PATH)
        self.assertEqual(calls, [config.PC_MATCH_LUT_OBJECT_PATH])
        with self.assertRaisesRegex(ValueError, "requires adopted L1"):
            load_for_experiment(
                "S1", lambda path: self.fail(path), adopted=("E2",)
            )

    def test_s1_profile_and_launch_contract_require_adopted_l1(self):
        with self.assertRaisesRegex(ValueError, "requires adopted L1"):
            profile_for("S1", "B", ("E2",))
        with self.assertRaisesRegex(ValueError, "requires adopted L1"):
            launch_contract_from_command_line(
                "-CGRProbe=MobilePCLikeAB -CGRExperiment=S1 "
                "-CGRAdopted=E2 -saveddirsuffix=MainMapMobilePCLikeS1AB"
            )

    def test_l1_changes_only_pc_match_lut_intensity(self):
        control = profile_for("L1", "A_PRE", ("E2",))
        candidate = profile_for("L1", "B", ("E2",))

        self.assertEqual(control["pc_match_lut_intensity"], 0.0)
        self.assertEqual(candidate["pc_match_lut_intensity"], 0.35)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"pc_match_lut_intensity"},
        )

    def test_c1_changes_only_facade_wall_chroma(self):
        control = profile_for("C1", "A_PRE", ("E2",))
        candidate = profile_for("C1", "B", ("E2",))

        self.assertEqual(control["wall_color_scale"], 0.92)
        self.assertEqual(control["wall_chroma"], 1.0)
        self.assertEqual(candidate["wall_chroma"], 1.12)
        self.assertEqual(
            {key for key in candidate if candidate[key] != control[key]},
            {"wall_chroma"},
        )

    def test_wall_color_transform_preserves_luminance_and_alpha(self):
        transform = _required_api("profile_wall_color")
        source = {"r": 0.8, "g": 0.3, "b": 0.1, "a": 0.7}
        profile = {
            "wall_color_scale": 0.78,
            "wall_chroma": 1.12,
        }

        result = transform(source, profile)

        self.assertAlmostEqual(result["r"], 0.8489768)
        self.assertAlmostEqual(result["g"], 0.2889768)
        self.assertAlmostEqual(result["b"], 0.0649768)
        self.assertEqual(result["a"], 0.7)
        source_luma = 0.2126 * 0.8 + 0.7152 * 0.3 + 0.0722 * 0.1
        result_luma = (
            0.2126 * result["r"]
            + 0.7152 * result["g"]
            + 0.0722 * result["b"]
        )
        self.assertAlmostEqual(result_luma, source_luma)

    def test_runtime_wall_multiplier_preserves_current_baseline(self):
        multiplier = _required_api("runtime_wall_multiplier")

        self.assertEqual(multiplier(profile_for("E1", "A_PRE", ())), 1.0)
        self.assertAlmostEqual(
            multiplier(profile_for("E2", "B", ())),
            0.92 / 0.78,
        )

    def test_only_each_time_control_frame_requires_live_settle(self):
        requires_live_settle = _required_api("requires_live_time_settle")

        self.assertTrue(requires_live_settle("A_PRE"))
        self.assertFalse(requires_live_settle("B"))
        self.assertFalse(requires_live_settle("A_POST"))

    def test_live_settle_requires_180_ticks_and_three_stable_samples(self):
        is_time_settled = _required_api("is_time_settled")

        self.assertFalse(is_time_settled(179, 3))
        self.assertFalse(is_time_settled(180, 2))
        self.assertTrue(is_time_settled(180, 3))

    def test_live_settle_timeout_is_after_600_ticks(self):
        is_time_settle_timed_out = _required_api("is_time_settle_timed_out")

        self.assertFalse(is_time_settle_timed_out(600))
        self.assertTrue(is_time_settle_timed_out(601))

    def test_capture_manual_exposure_is_explicit_for_all_times(self):
        expected_exposure = _required_api("expected_capture_exposure")

        for time_code in ("0900", "1200", "1600"):
            with self.subTest(time_code=time_code):
                self.assertEqual(
                    expected_exposure(time_code),
                    {
                        "method_override": True,
                        "method": "<AutoExposureMethod.AEM_MANUAL: 2>",
                        "bias_override": True,
                        "bias": -0.15,
                        "method_cvar": 2,
                    },
                )

    def test_directional_expectations_are_time_specific(self):
        expected_directional = _required_api("expected_directional")
        expected = {
            "0900": -36.0,
            "1200": -90.0,
            "1600": -38.57142639160157,
        }

        for time_code, pitch in expected.items():
            with self.subTest(time_code=time_code):
                self.assertEqual(
                    expected_directional(time_code),
                    {
                        "pitch": pitch,
                        "effective_azimuth": 35.0,
                        "intensity": 0.8,
                        "use_temperature": True,
                        "temperature": 6500.0,
                    },
                )

    def test_directional_stability_needs_three_consecutive_close_samples(self):
        next_stable_count = _required_api("next_directional_stable_count")
        expected_directional = _required_api("expected_directional")
        sample = expected_directional("0900")

        count = 0
        for _ in range(3):
            count = next_stable_count(count, sample, "0900")
        self.assertEqual(count, 3)
        drifted = dict(sample, pitch=-34.0)
        self.assertEqual(next_stable_count(count, drifted, "0900"), 0)

    def test_e1_changes_only_skylight(self):
        a = profile_for("E1", "A_PRE", ())
        b = profile_for("E1", "B", ())

        self.assertEqual(a["sky_light_intensity"], 0.50)
        self.assertEqual(b["sky_light_intensity"], 0.35)
        self.assertEqual(
            {key for key in b if b[key] != a[key]},
            {"sky_light_intensity"},
        )

    def test_adopted_candidates_accumulate_before_current_b(self):
        a = profile_for("E3", "A_PRE", ("E1", "E2"))
        b = profile_for("E3", "B", ("E1", "E2"))

        self.assertEqual(a["sky_light_intensity"], 0.35)
        self.assertEqual(a["wall_color_scale"], 0.92)
        self.assertEqual(a["windows_roughness"], 0.40)
        self.assertEqual(b["windows_roughness"], 0.30)

    def test_rejects_future_or_out_of_order_adoption(self):
        for adopted in (("E3",), ("E2", "E1"), ("E1", "E1")):
            with self.subTest(adopted=adopted):
                with self.assertRaises(ValueError):
                    profile_for("E2", "B", adopted)

    def test_rejected_prior_experiments_may_be_omitted(self):
        profile = profile_for("E4", "A_PRE", ("E1", "E3"))

        self.assertEqual(profile["sky_light_intensity"], 0.35)
        self.assertEqual(profile["wall_color_scale"], 0.78)
        self.assertEqual(profile["windows_roughness"], 0.30)

    def test_each_b_variant_changes_only_its_candidate(self):
        expected = {
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
            "E5": ("reflection_mode", "window_mask_energy_lerp"),
            "E6": ("reflection_capture", True),
        }
        for experiment, (key, value) in expected.items():
            with self.subTest(experiment=experiment):
                adopted = (
                    ("E2", "L1")
                    if experiment in ("S2", "S3", "R1", "L2")
                    else (("L1",) if experiment == "S1" else ())
                )
                control = profile_for(experiment, "A_POST", adopted)
                candidate = profile_for(experiment, "B", adopted)
                self.assertEqual(candidate[key], value)
                self.assertEqual(
                    {name for name in candidate if candidate[name] != control[name]},
                    {key},
                )

    def test_rejects_unknown_experiment_or_variant(self):
        with self.assertRaises(ValueError):
            profile_for("E7", "B", ())
        with self.assertRaises(ValueError):
            profile_for("E1", "UNKNOWN", ())

    def test_parses_experiment_and_adopted_flags(self):
        command_line = "Game.exe -CGRExperiment=E3 -CGRAdopted=E1,E2 -log"

        self.assertEqual(experiment_from_command_line(command_line), "E3")
        self.assertEqual(adopted_from_command_line(command_line), ("E1", "E2"))

    def test_parsers_are_case_insensitive_and_adopted_defaults_to_empty(self):
        command_line = "Game.exe -cgrexperiment=e2 -cgradopted=e1"

        self.assertEqual(experiment_from_command_line(command_line), "E2")
        self.assertEqual(adopted_from_command_line(command_line), ("E1",))
        self.assertEqual(adopted_from_command_line("Game.exe -log"), ())
        self.assertEqual(adopted_from_command_line("Game.exe -CGRAdopted="), ())

    def test_missing_experiment_is_rejected(self):
        with self.assertRaises(ValueError):
            experiment_from_command_line("-vulkan -FeatureLevelES31")

    def test_saved_suffix_is_unique_per_experiment(self):
        self.assertEqual(saved_suffix_for("E1"), "MainMapMobilePCLikeE1AB")
        self.assertEqual(saved_suffix_for("C1"), "MainMapMobilePCLikeC1AB")
        self.assertEqual(saved_suffix_for("L1"), "MainMapMobilePCLikeL1AB")
        self.assertEqual(saved_suffix_for("S1"), "MainMapMobilePCLikeS1AB")
        self.assertEqual(saved_suffix_for("S2"), "MainMapMobilePCLikeS2AB")
        self.assertEqual(saved_suffix_for("L2"), "MainMapMobilePCLikeL2AB")
        self.assertEqual(saved_suffix_for("E5"), "MainMapMobilePCLikeE5AB")

    def test_capture_support_gate_rejects_deferred_experiments(self):
        for experiment in (
            "E1", "E2", "E3", "E4", "C1", "L1", "S1", "S2", "S3", "R1", "L2"
        ):
            with self.subTest(experiment=experiment):
                self.assertEqual(ensure_capture_supported(experiment), experiment)
        for experiment in ("E5", "E6"):
            with self.subTest(experiment=experiment):
                with self.assertRaisesRegex(ValueError, "dedicated task"):
                    ensure_capture_supported(experiment)

    def test_launch_contract_requires_matching_unique_tokens(self):
        command_line = (
            "-CGRProbe=MobilePCLikeAB -CGRExperiment=E3 "
            "-CGRAdopted=E1,E2 -saveddirsuffix=MainMapMobilePCLikeE3AB"
        )
        self.assertEqual(
            launch_contract_from_command_line(command_line),
            ("E3", ("E1", "E2")),
        )
        invalid = (
            command_line.replace("MobilePCLikeAB", "UnknownProbe"),
            command_line + " -CGRProbe=MobilePCLikeAB",
            command_line.replace(" -CGRAdopted=E1,E2", ""),
            command_line.replace("E1,E2", "E2,E1"),
            command_line.replace("MainMapMobilePCLikeE3AB", "MainMapMobilePCLikeE2AB"),
            command_line + " -saveddirsuffix=MainMapMobilePCLikeE3AB",
        )
        for invalid_command_line in invalid:
            with self.subTest(command_line=invalid_command_line):
                with self.assertRaises(ValueError):
                    launch_contract_from_command_line(invalid_command_line)

    def test_adopted_parser_rejects_empty_csv_items(self):
        for payload in ("E1,,E2", "E1,", ",E1"):
            with self.subTest(payload=payload):
                with self.assertRaises(ValueError):
                    adopted_from_command_line("Game.exe -CGRAdopted=" + payload)

    def test_parsers_fail_closed_on_ambiguous_or_invalid_flags(self):
        invalid_cases = (
            (
                experiment_from_command_line,
                "Game.exe -CGRExperiment=E1 -CGRExperiment=E2",
            ),
            (experiment_from_command_line, "Game.exe -CGRExperiment=E7"),
            (adopted_from_command_line, "Game.exe -CGRAdopted=E1,E1"),
            (adopted_from_command_line, "Game.exe -CGRAdopted=E2,E1"),
            (adopted_from_command_line, "Game.exe -CGRAdopted=E7"),
        )
        for parser, command_line in invalid_cases:
            with self.subTest(command_line=command_line):
                with self.assertRaises(ValueError):
                    parser(command_line)

    def test_capture_sequence_has_exact_nine_frame_order(self):
        self.assertEqual(
            capture_sequence(),
            (
                ("A_PRE", "0900"),
                ("B", "0900"),
                ("A_POST", "0900"),
                ("A_PRE", "1200"),
                ("B", "1200"),
                ("A_POST", "1200"),
                ("A_PRE", "1600"),
                ("B", "1600"),
                ("A_POST", "1600"),
            ),
        )

    def test_cf1_sequences_are_stage_specific(self):
        self.assertEqual(
            capture_sequence("CF1", "BASELINE_SAFETY"),
            tuple(
                ("A_PRE", code)
                for code in ("1800", "1930", "0000", "0530")
            ),
        )
        self.assertEqual(
            capture_sequence("CF1", "AB"),
            tuple(
                (variant, code)
                for code in ("0900", "1800", "1930", "0000", "0530")
                for variant in ("A_PRE", "B", "A_POST")
            ),
        )

    def test_legacy_sequence_and_exposure_are_unchanged(self):
        times_for = _required_api("times_for")
        expected_exposure = _required_api("expected_capture_exposure")

        self.assertEqual(times_for("L2"), ("0900", "1200", "1600"))
        self.assertEqual(len(capture_sequence("L2")), 9)
        self.assertEqual(expected_exposure("L2", "1200")["bias"], -0.15)
        self.assertEqual(expected_exposure("1200")["bias"], -0.15)

    def test_cf1_registry_stack_maps_explicitly_to_runtime_profile(self):
        runtime_adopted = _required_api("runtime_adopted_for")
        stack = ("N1DIM", "AZ35", "TCGB", "SH04", "E2", "L2")

        self.assertEqual(runtime_adopted("CF1", stack), ("E2", "L2"))
        for changed in (
            stack[:-1],
            ("N1DIM", "AZ35", "TCGB", "SH04", "L2", "E2"),
            stack + ("EXTRA",),
        ):
            with self.subTest(changed=changed):
                with self.assertRaises(ValueError):
                    runtime_adopted("CF1", changed)

    def test_cf1_profile_uses_exact_e2_l2_runtime_baseline(self):
        control = profile_for("CF1", "A_PRE", ("E2", "L2"), "1930")
        candidate = profile_for("CF1", "B", ("E2", "L2"), "1930")

        self.assertEqual(control["wall_color_scale"], 0.92)
        self.assertEqual(control["pc_match_lut_intensity"], 1.0)
        self.assertEqual(
            {key for key in control if control[key] != candidate[key]},
            {"color_saturation_midtones"},
        )
        for adopted in (("E2",), ("E2", "L1"), ("E2", "L2", "S1")):
            with self.subTest(adopted=adopted):
                with self.assertRaisesRegex(
                    ValueError, "CF1 requires exact adopted baseline E2,L2"
                ):
                    profile_for("CF1", "A_PRE", adopted, "1930")

    def test_cf1_loads_the_immutable_l2_lut_and_is_capture_supported(self):
        load_lut = _required_api("load_pc_match_lut_for_experiment")

        self.assertEqual(
            load_lut("CF1", lambda path: path, adopted=("E2", "L2")),
            config.PC_MATCH_LUT_OBJECT_PATH,
        )
        self.assertEqual(ensure_capture_supported("CF1"), "CF1")
        self.assertEqual(config.ORDER.index("CF1"), config.ORDER.index("L2") + 1)

    def test_cf1_directional_and_exposure_route_to_the_five_time_contract(self):
        expected_exposure = _required_api("expected_capture_exposure")
        expected_directional = _required_api("expected_directional")

        self.assertEqual(expected_exposure("CF1", "1930")["bias"], 0.20)
        directional = expected_directional("CF1", "1800")
        self.assertAlmostEqual(directional["pitch"], -12.8571428571, places=6)
        self.assertAlmostEqual(directional["intensity"], 0.44, places=6)
        self.assertFalse(directional["cast_shadows"])
        self.assertFalse(directional["cast_dynamic_shadows"])

    def test_cf1_legacy_launch_parser_validates_without_an_implicit_time(self):
        command_line = (
            "Game.exe -CGRProbe=MobilePCLikeAB -CGRExperiment=CF1 "
            "-CGRAdopted=E2,L2 -saveddirsuffix=MainMapMobilePCLikeCF1AB"
        )

        self.assertEqual(
            launch_contract_from_command_line(command_line),
            ("CF1", ("E2", "L2")),
        )

    def test_cf1_directional_stability_uses_the_cf1_time_contract(self):
        stable_count = _required_api("next_directional_stable_count")
        readback = _required_api("expected_directional")("CF1", "1930")

        self.assertEqual(
            stable_count(2, readback, "1930", experiment="CF1"),
            3,
        )

    def test_cf1_live_stability_includes_directional_skylight_and_exposure(self):
        stable_count = _required_api("next_cf1_lighting_stable_count")
        directional = _required_api("expected_directional")("CF1", "1930")
        exposure = _required_api("expected_capture_exposure")(
            "CF1", "1930"
        )
        readback = {
            "directional": directional,
            "sky_light_intensity": 0.20,
            "sky_light_color": list(
                _required_api("cf1_expected_lighting")("1930")[
                    "sky_light_color"
                ]
            ),
            "exposure": exposure,
        }

        self.assertEqual(stable_count(2, readback, "1930"), 3)
        readback["sky_light_intensity"] = 0.25
        self.assertEqual(stable_count(2, readback, "1930"), 0)
        readback["sky_light_intensity"] = 0.20
        readback["sky_light_color"][0] += 0.05
        self.assertEqual(stable_count(2, readback, "1930"), 0)


if __name__ == "__main__":
    unittest.main()
