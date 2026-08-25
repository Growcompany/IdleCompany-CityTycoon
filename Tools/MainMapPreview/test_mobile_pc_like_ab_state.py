import json
import os
import tempfile
import unittest

try:
    import mobile_pc_like_ab_state as capture_state
except ImportError:
    capture_state = None


def _required_api(name):
    if capture_state is None:
        raise AssertionError("missing required module: mobile_pc_like_ab_state")
    value = getattr(capture_state, name, None)
    if value is None:
        raise AssertionError("missing required API: " + name)
    return value


class MobilePcLikeAbStateTests(unittest.TestCase):
    def test_rotation_snapshot_is_detached_from_live_wrapper_mutation(self):
        snapshot_rotation_values = _required_api("snapshot_rotation_values")

        class LiveRotation:
            pitch = -56.114288330078146
            yaw = 180.0
            roll = -180.0

        live_rotation = LiveRotation()
        saved_rotation = snapshot_rotation_values(live_rotation)

        live_rotation.pitch = -56.86428833007816

        self.assertEqual(
            saved_rotation,
            {
                "pitch": -56.114288330078146,
                "yaw": 180.0,
                "roll": -180.0,
            },
        )

    def test_fresh_rotator_maps_saved_axes_by_keyword_each_time(self):
        build_fresh_rotator = _required_api("build_fresh_rotator")
        calls = []

        def rotator_factory(**kwargs):
            calls.append(kwargs)
            return object()

        saved_rotation = {
            "pitch": -56.114288330078146,
            "yaw": 180.0,
            "roll": -180.0,
        }
        first = build_fresh_rotator(saved_rotation, rotator_factory)
        second = build_fresh_rotator(saved_rotation, rotator_factory)

        self.assertIsNot(first, second)
        self.assertEqual(
            calls,
            [
                {
                    "roll": -180.0,
                    "pitch": -56.114288330078146,
                    "yaw": 180.0,
                },
                {
                    "roll": -180.0,
                    "pitch": -56.114288330078146,
                    "yaw": 180.0,
                },
            ],
        )

    def test_restore_pins_numeric_rotation_after_normal_restore_before_readback(self):
        restore_then_pin_numeric_rotation = _required_api(
            "restore_then_pin_numeric_rotation"
        )
        calls = []

        def normal_restore():
            calls.append("normal_restore")
            return []

        def pin_numeric_rotation():
            calls.append("pin_numeric_rotation")

        def readback():
            calls.append("readback")
            return {"directional": {"pitch": -58.83927917480474}}

        errors, restored = restore_then_pin_numeric_rotation(
            normal_restore,
            pin_numeric_rotation,
            readback,
        )

        self.assertEqual(
            calls, ["normal_restore", "pin_numeric_rotation", "readback"]
        )
        self.assertEqual(errors, [])
        self.assertEqual(
            restored, {"directional": {"pitch": -58.83927917480474}}
        )

    def test_rotation_pin_failure_is_a_named_restore_failure(self):
        restore_then_pin_numeric_rotation = _required_api(
            "restore_then_pin_numeric_rotation"
        )

        def fail_pin():
            raise RuntimeError("setter rejected rotation")

        errors, restored = restore_then_pin_numeric_rotation(
            lambda: [], fail_pin, lambda: {"directional": {"pitch": -59.589}}
        )

        self.assertEqual(
            errors,
            ["directional relative rotation: setter rejected rotation"],
        )
        restore_verified = not errors and restored["directional"][
            "pitch"
        ] == -58.839
        self.assertFalse(restore_verified)
        self.assertEqual(restored["directional"]["pitch"], -59.589)

    def test_directional_restore_requires_exact_readback_with_tight_tolerance(self):
        directional_restore_differences = _required_api(
            "directional_restore_differences"
        )
        original = {
            "pitch": -58.83927917480474,
            "yaw": 180.0,
            "roll": -180.0,
            "effective_azimuth": 0.0,
        }
        exact = dict(original)
        derived = dict(original)
        derived["pitch"] = -59.58927917480473

        self.assertEqual(
            directional_restore_differences(exact, original),
            [],
        )
        self.assertEqual(
            directional_restore_differences(derived, original),
            [{
                "field": "directional.pitch",
                "expected": -58.83927917480474,
                "actual": -59.58927917480473,
            }],
        )

    def test_directional_rotation_axes_use_one_canonical_boundary(self):
        normalize_axis = _required_api("normalize_axis")

        self.assertEqual(normalize_axis(-180.0), 180.0)
        self.assertEqual(normalize_axis(180.0), 180.0)
        self.assertEqual(normalize_axis(-540.0), 180.0)
        self.assertEqual(normalize_axis(540.0), 180.0)

    def test_directional_restore_accepts_semantically_equal_180_axes(self):
        directional_restore_differences = _required_api(
            "directional_restore_differences"
        )
        expected = {
            "pitch": -180.0,
            "yaw": 180.0,
            "roll": -180.0,
            "effective_azimuth": 180.0,
        }
        actual = {
            "pitch": 180.0,
            "yaw": -180.0,
            "roll": 180.0,
            "effective_azimuth": -180.0,
        }

        self.assertEqual(
            directional_restore_differences(actual, expected), []
        )

    def test_directional_restore_uses_shortest_angle_tolerance(self):
        directional_restore_differences = _required_api(
            "directional_restore_differences"
        )
        expected = {"yaw": 179.99975}
        within = {"yaw": -179.99975}
        outside = {"yaw": -179.99825}

        self.assertEqual(
            directional_restore_differences(within, expected), []
        )
        self.assertEqual(
            directional_restore_differences(outside, expected),
            [{
                "field": "directional.yaw",
                "expected": 179.99975,
                "actual": -179.99825,
            }],
        )

    def test_diagnostic_write_failure_keeps_primary_error_and_records_cleanup(self):
        raise_after_diagnostic = _required_api("raise_after_diagnostic")
        cleanup_errors = []

        def fail_write():
            raise OSError("disk full")

        with self.assertRaisesRegex(
            RuntimeError, "active readback mismatch at A_PRE_0900"
        ):
            raise_after_diagnostic(
                RuntimeError("active readback mismatch at A_PRE_0900"),
                fail_write,
                cleanup_errors,
            )

        self.assertEqual(cleanup_errors, ["failure diagnostic: disk full"])

    def test_live_settle_completion_reads_live_then_freezes_in_same_call(self):
        complete_live_settle = _required_api("complete_live_settle")
        calls = []

        def read_live():
            calls.append("read_live")
            return {"time_dilation": 1.0}

        def freeze_now():
            calls.append("freeze_now")

        readback = complete_live_settle(read_live, freeze_now)

        self.assertEqual(calls, ["read_live", "freeze_now"])
        self.assertEqual(readback, {"time_dilation": 1.0})

    def test_failure_diagnostic_carries_restore_pending_context(self):
        build_failure_diagnostic = _required_api("build_failure_diagnostic")
        context = {
            "phase": "READBACK",
            "sequence_index": 0,
            "label": "A_PRE_0900",
            "attempted_readback": {"directional": {"pitch": -30.0}},
            "field_differences": [{"field": "directional.pitch"}],
            "restore_pending": True,
        }

        self.assertEqual(
            build_failure_diagnostic("run-1", context),
            {
                "ok": False,
                "run_id": "run-1",
                "restore_pending": True,
                "phase": "READBACK",
                "sequence_index": 0,
                "label": "A_PRE_0900",
                "attempted_readback": {"directional": {"pitch": -30.0}},
                "field_differences": [{"field": "directional.pitch"}],
                "failure_context": context,
            },
        )

    def test_capture_phase_order_thaws_only_control_frames(self):
        phase_plan = _required_api("capture_phase_plan")

        self.assertEqual(
            phase_plan("A_PRE"),
            (
                "PREPARE_TIME",
                "SETTLE_TIME_LIVE",
                "FREEZE",
                "APPLY_VARIANT",
                "SETTLE_VARIANT",
                "READBACK",
                "CAPTURE",
                "WAIT",
            ),
        )
        frozen = (
            "APPLY_VARIANT",
            "SETTLE_VARIANT",
            "READBACK",
            "CAPTURE",
            "WAIT",
        )
        self.assertEqual(phase_plan("B"), frozen)
        self.assertEqual(phase_plan("A_POST"), frozen)

    def test_capture_phase_transition_follows_the_variant_plan(self):
        phase_plan = _required_api("capture_phase_plan")
        next_capture_phase = _required_api("next_capture_phase")

        for variant in ("A_PRE", "B", "A_POST"):
            plan = phase_plan(variant)
            for current, expected in zip(plan, plan[1:]):
                with self.subTest(variant=variant, current=current):
                    self.assertEqual(
                        next_capture_phase(variant, current), expected
                    )
            self.assertIsNone(next_capture_phase(variant, plan[-1]))

    def test_restore_phase_plan_settles_live_before_final_verification(self):
        restore_phase_plan = _required_api("restore_phase_plan")

        self.assertEqual(
            restore_phase_plan(),
            ("RESTORE_PREPARE", "RESTORE_SETTLE_LIVE", "RESTORE_FINAL"),
        )

    def test_named_readback_differences_report_exact_directional_fields(self):
        readback_differences = _required_api("readback_differences")
        expected = {
            "directional": {
                "pitch": -36.0,
                "effective_azimuth": 35.0,
                "intensity": 0.8,
                "use_temperature": True,
                "temperature": 6500.0,
            }
        }
        actual = {
            "directional": {
                "pitch": -30.0,
                "effective_azimuth": 0.0,
                "intensity": 0.4,
                "use_temperature": False,
                "temperature": 5000.0,
            }
        }

        self.assertEqual(
            [entry["field"] for entry in readback_differences(actual, expected)],
            [
                "directional.effective_azimuth",
                "directional.intensity",
                "directional.pitch",
                "directional.temperature",
                "directional.use_temperature",
            ],
        )

    def test_readback_differences_applies_numeric_tolerance_inside_sequences(self):
        readback_differences = _required_api("readback_differences")
        expected = {
            "window_emissive": [
                {
                    "path": "/Game/Facade/A",
                    "parameters": (
                        {"name": "Night_Intensity", "value": 0.75},
                    ),
                }
            ]
        }
        actual = {
            "window_emissive": [
                {
                    "path": "/Game/Facade/A",
                    "parameters": (
                        {
                            "name": "Night_Intensity",
                            "value": 0.75000002980232,
                        },
                    ),
                }
            ]
        }

        self.assertEqual(
            readback_differences(actual, expected, tolerance=0.000001),
            [],
        )

    def test_readback_differences_reports_indexed_numeric_sequence_drift(self):
        readback_differences = _required_api("readback_differences")
        expected = {
            "window_emissive": [
                {"parameters": ({"value": 0.75},)},
            ]
        }
        actual = {
            "window_emissive": [
                {"parameters": ({"value": 0.7502},)},
            ]
        }

        self.assertEqual(
            readback_differences(actual, expected, tolerance=0.000001),
            [
                {
                    "field": "window_emissive[0].parameters[0].value",
                    "expected": 0.75,
                    "actual": 0.7502,
                }
            ],
        )

    def test_readback_differences_fails_closed_on_sequence_shape_and_order(self):
        readback_differences = _required_api("readback_differences")
        expected_items = [
            {"path": "/Game/Facade/A", "value": 0.75},
            {"path": "/Game/Facade/B", "value": 0.75},
        ]
        cases = (
            (
                "missing",
                [expected_items[0]],
                ["window_emissive[1]"],
            ),
            (
                "extra",
                expected_items + [{"path": "/Game/Facade/C", "value": 0.75}],
                ["window_emissive[2]"],
            ),
            (
                "type",
                tuple(expected_items),
                ["window_emissive"],
            ),
            (
                "order",
                list(reversed(expected_items)),
                [
                    "window_emissive[0].path",
                    "window_emissive[1].path",
                ],
            ),
        )

        for label, actual_items, expected_fields in cases:
            with self.subTest(label=label):
                differences = readback_differences(
                    {"window_emissive": actual_items},
                    {"window_emissive": expected_items},
                )
                self.assertEqual(
                    [entry["field"] for entry in differences],
                    expected_fields,
                )

    def test_atomic_failure_json_replaces_target_with_complete_payload(self):
        atomic_write_json = _required_api("atomic_write_json")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = os.path.join(temp_dir, "failure_run.json")
            with open(path, "w", encoding="utf-8") as stream:
                stream.write("stale")

            atomic_write_json(path, {"ok": False, "failure_context": {"x": 1}})

            self.assertFalse(os.path.exists(path + ".tmp"))
            with open(path, "r", encoding="utf-8") as stream:
                self.assertEqual(
                    json.load(stream),
                    {"ok": False, "failure_context": {"x": 1}},
                )

    def test_atomic_no_clobber_json_publish_never_replaces_old_evidence(self):
        publish = _required_api("atomic_write_json_if_absent")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = os.path.join(temp_dir, "result_marker.json")
            with open(path, "wb") as stream:
                stream.write(b"old evidence")

            with self.assertRaises(FileExistsError):
                publish(path, {"ok": False})

            with open(path, "rb") as stream:
                self.assertEqual(stream.read(), b"old evidence")
            self.assertEqual(
                [name for name in os.listdir(temp_dir) if ".publish-" in name],
                [],
            )

            fresh = os.path.join(temp_dir, "fresh_marker.json")
            publish(fresh, {"ok": True})
            with open(fresh, "r", encoding="utf-8") as stream:
                self.assertEqual(json.load(stream), {"ok": True})


class Cf1CaptureStateContractTests(unittest.TestCase):
    RUN_ID = "12345678-1234-1234-1234-1234567890ab"
    SEED_SHA = "a" * 64

    def _run_paths(self, temp_dir, stage="BASELINE_SAFETY"):
        return _required_api("cf1_run_paths")(
            stage, self.RUN_ID, temp_dir
        )

    def _valid_runtime_state(self, temp_dir):
        paths = self._run_paths(temp_dir)
        return {
            "project_saved_dir": paths["expected_project_saved_dir"],
            "expected_project_saved_dir": paths["expected_project_saved_dir"],
            "seed_slot_sha256": self.SEED_SHA,
            "isolated_slot_sha256": self.SEED_SHA,
            "root_slot_sha256": self.SEED_SHA,
            "wipe_save_on_launch": False,
            "start_mode": 0,
            "save_exists": True,
            "load_succeeded": True,
            "experiment": "CF1",
            "adopted": ["E2", "L2"],
            "renderer": "Vulkan",
            "feature_level": "ES3_1",
            "saved_dir_suffix": paths["saved_dir_suffix"],
        }

    def _active_readback(self, midtone):
        return {
            "grading": {
                "color_saturation": {
                    "override": True,
                    "value": [0.92, 0.92, 0.92, 1.0],
                },
                "color_contrast": {
                    "override": True,
                    "value": [1.02, 1.02, 1.02, 1.0],
                },
                "color_gain": {
                    "override": True,
                    "value": [1.02, 1.05, 1.10, 1.0],
                },
                "color_saturation_midtones": {
                    "override": True,
                    "value": list(midtone),
                },
                "color_correction_shadows_max": {
                    "override": True,
                    "value": 0.09,
                },
                "color_correction_highlights_min": {
                    "override": True,
                    "value": 0.50,
                },
            },
            "exposure": {
                "method_override": True,
                "method": "<AutoExposureMethod.AEM_MANUAL: 2>",
                "bias_override": True,
                "bias": 0.20,
            },
            "film": {
                "scene_color_tint": {
                    "override": False,
                    "value": [1.0, 1.0, 1.0, 1.0],
                },
                "slope": {"override": False, "value": 0.88},
                "toe": {"override": False, "value": 0.55},
            },
            "lut": {"lut_override": True, "intensity": 1.0},
            "bloom": {"intensity": 0.30, "threshold": 3.0},
            "sharpen": {"value": 0.4},
            "vignette": {"override": False, "intensity": 0.4},
            "window_emissive": [{"material": "M1", "scale": 0.75}],
            "mpc_window_light": {"night_intensity": 1.0},
            "reflection": {"strength": 0.45},
            "directional": {"cast_shadows": False},
            "camera": {"fov": 27.85503},
            "time": {"hours": 19, "minutes": 30, "seconds": 0},
        }

    def _loaded_scene(self):
        return {
            "schema_version": 1,
            "map_object_path": (
                "/Game/CompanyGrowth/Level/MainMap_TheRiverwalkCity."
                "MainMap_TheRiverwalkCity"
            ),
            "city_buildings": [{
                "city_key": 1,
                "class_object_path": "/Game/City/BP_MB001.BP_MB001_C",
                "transform": {
                    "location": [1.0, -0.0, 3.0],
                    "rotation": [-180.0, 180.0, 540.0],
                    "scale": [1.0, 1.0, 1.0],
                },
            }],
            "plots": [{
                "plot_id": "Plot_01",
                "owned": True,
                "transform": {
                    "location": [4.0, 5.0, 6.0],
                    "rotation": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                },
            }],
            "placed_buildings": [{
                "building_index": 7,
                "building_id": "HQ",
                "plot_id": "Plot_01",
                "company_type": 2,
                "body_module_copies": 3,
                "floor_height_body_module_scale": 1.0,
                "uv_layout_selection": 0,
                "walls_between_windows_switch": 1.0,
                "applied_skin_id": 100,
                "applied_light_id": 100,
                "transform": {
                    "location": [4.0, 5.0, 6.0],
                    "rotation": [0.0, 0.0, 0.0],
                    "scale": [1.0, 1.0, 1.0],
                },
            }],
        }

    def test_baseline_safety_never_contains_candidate_or_post(self):
        plan = _required_api("experiment_phase_plan")(
            "CF1", "BASELINE_SAFETY"
        )
        self.assertEqual(
            plan,
            (
                ("A_PRE", "1800"),
                ("A_PRE", "1930"),
                ("A_PRE", "0000"),
                ("A_PRE", "0530"),
            ),
        )
        self.assertNotIn("B", repr(plan))
        self.assertNotIn("A_POST", repr(plan))

    def test_cf1_ab_is_the_exact_immutable_fifteen_frame_plan(self):
        plan = _required_api("experiment_phase_plan")("CF1", "AB")
        self.assertIsInstance(plan, tuple)
        self.assertEqual(len(plan), 15)
        self.assertEqual(
            plan[:6],
            (
                ("A_PRE", "0900"),
                ("B", "0900"),
                ("A_POST", "0900"),
                ("A_PRE", "1800"),
                ("B", "1800"),
                ("A_POST", "1800"),
            ),
        )

    def test_cf1_stage_and_run_id_are_canonical_and_case_strict(self):
        canonical_stage = _required_api("canonical_cf1_stage")
        canonical_run_id = _required_api("canonical_cf1_run_id")

        self.assertEqual(canonical_stage("AB"), "AB")
        self.assertEqual(canonical_stage("BASELINE_SAFETY"), "BASELINE_SAFETY")
        self.assertEqual(canonical_run_id(self.RUN_ID), self.RUN_ID)
        for stage in ("ab", "baseline_safety", " AB", "AB "):
            with self.subTest(stage=stage), self.assertRaises(ValueError):
                canonical_stage(stage)
        for run_id in (
            self.RUN_ID.upper(),
            self.RUN_ID.replace("-", ""),
            "{" + self.RUN_ID + "}",
        ):
            with self.subTest(run_id=run_id), self.assertRaises(ValueError):
                canonical_run_id(run_id)

    def test_cf1_marker_log_and_images_are_run_scoped_and_collision_safe(self):
        collision_errors = _required_api("cf1_evidence_collision_errors")
        with tempfile.TemporaryDirectory() as temp_dir:
            paths = self._run_paths(temp_dir)
            expected_tail = os.path.join(
                "Saved", "BuildingLookdev", "MainMapPreview",
                "CinematicFilterStrongAB", "CF1", "BASELINE_SAFETY",
                self.RUN_ID, "VulkanES31",
            )
            self.assertTrue(paths["output_dir"].endswith(expected_tail))
            self.assertEqual(
                os.path.basename(paths["marker"]), "result_marker.json"
            )
            self.assertEqual(os.path.basename(paths["log"]), "capture.log")
            self.assertEqual(
                [os.path.basename(path) for path in paths["images"]],
                [
                    "MainMap_CF1_A_PRE_1800.png",
                    "MainMap_CF1_A_PRE_1930.png",
                    "MainMap_CF1_A_PRE_0000.png",
                    "MainMap_CF1_A_PRE_0530.png",
                ],
            )
            self.assertEqual(collision_errors(paths), [])
            os.makedirs(paths["output_dir"], exist_ok=True)
            with open(paths["log"], "wb") as stream:
                stream.write(b"owned process log")
            self.assertEqual(collision_errors(paths), [])
            with open(paths["marker"], "wb") as stream:
                stream.write(b"old")
            self.assertEqual(
                collision_errors(paths), [paths["marker"]]
            )

    def test_later_ab_run_cannot_change_prior_safety_marker_or_log_bytes(self):
        fingerprints = _required_api("cf1_evidence_fingerprints")
        with tempfile.TemporaryDirectory() as temp_dir:
            safety = self._run_paths(temp_dir, "BASELINE_SAFETY")
            ab = self._run_paths(temp_dir, "AB")
            os.makedirs(safety["output_dir"], exist_ok=True)
            with open(safety["marker"], "wb") as stream:
                stream.write(b"safety marker")
            with open(safety["log"], "wb") as stream:
                stream.write(b"safety log")
            before = fingerprints(safety)

            os.makedirs(ab["output_dir"], exist_ok=True)
            with open(ab["marker"], "wb") as stream:
                stream.write(b"AB marker")
            with open(ab["log"], "wb") as stream:
                stream.write(b"AB log")

            self.assertEqual(fingerprints(safety), before)
            self.assertNotEqual(safety["marker"], ab["marker"])
            self.assertNotEqual(safety["log"], ab["log"])

    def test_cf1_launch_contract_requires_unique_seed_stage_run_and_suffix(self):
        parse = _required_api("cf1_launch_contract_from_command_line")
        suffix = "CF1BaselineSafety" + self.RUN_ID.replace("-", "")
        command = (
            "Game.exe -CGRProbe=MobilePCLikeAB -CGRExperiment=CF1 "
            "-CGRAdopted=E2,L2 -CGRStage=BASELINE_SAFETY "
            "-CGRRunId=%s -CGRSeedSlotSha256=%s "
            "-saveddirsuffix=%s -vulkan -FeatureLevelES31"
            % (self.RUN_ID, self.SEED_SHA, suffix)
        )
        parsed = parse(command)
        self.assertEqual(parsed["stage"], "BASELINE_SAFETY")
        self.assertEqual(parsed["run_id"], self.RUN_ID)
        self.assertEqual(parsed["seed_slot_sha256"], self.SEED_SHA)
        self.assertEqual(parsed["saved_dir_suffix"], suffix)
        for invalid in (
            command + " -CGRStage=AB",
            command + " -CGRRunId=" + self.RUN_ID,
            command + " -CGRSeedSlotSha256=" + self.SEED_SHA,
            command.replace("-vulkan", ""),
            command.replace("-FeatureLevelES31", ""),
            command + " -d3d12",
            command + " -FeatureLevelSM5",
            command.replace("-CGRAdopted=E2,L2", "-CGRAdopted=E2,L1"),
        ):
            with self.subTest(command=invalid), self.assertRaises(ValueError):
                parse(invalid)

    def test_runner_contract_routes_strict_cf1_without_legacy_fixed_suffix(self):
        route = _required_api("runner_launch_contract_from_command_line")
        suffix = "CF1BaselineSafety" + self.RUN_ID.replace("-", "")
        command = (
            "Game.exe -CGRProbe=MobilePCLikeAB -CGRExperiment=CF1 "
            "-CGRAdopted=E2,L2 -CGRStage=BASELINE_SAFETY "
            "-CGRRunId=%s -CGRSeedSlotSha256=%s "
            "-saveddirsuffix=%s -vulkan -FeatureLevelES31"
            % (self.RUN_ID, self.SEED_SHA, suffix)
        )
        legacy_calls = []

        def legacy_parser(value):
            legacy_calls.append(value)
            raise AssertionError("strict CF1 must not use the legacy suffix parser")

        experiment, adopted, strict = route(command, legacy_parser)

        self.assertEqual(experiment, "CF1")
        self.assertEqual(adopted, ("E2", "L2"))
        self.assertEqual(strict["saved_dir_suffix"], suffix)
        self.assertEqual(legacy_calls, [])

        legacy_result = ("L2", ("E2", "L1"))
        self.assertEqual(
            route("legacy command", lambda value: legacy_result),
            ("L2", ("E2", "L1"), None),
        )

    def test_cf1_runtime_gate_rejects_saved_dir_seed_config_and_load_drift(self):
        gate_errors = _required_api("cf1_runtime_state_errors")
        with tempfile.TemporaryDirectory() as temp_dir:
            valid = self._valid_runtime_state(temp_dir)
            self.assertEqual(gate_errors(valid), [])
            cases = {
                "project_saved_dir": "wrong",
                "isolated_slot_sha256": "b" * 64,
                "root_slot_sha256": "b" * 64,
                "wipe_save_on_launch": True,
                "start_mode": 1,
                "save_exists": False,
                "load_succeeded": False,
                "renderer": "D3D12",
                "feature_level": "SM5",
                "saved_dir_suffix": "wrong",
            }
            for field, changed in cases.items():
                state = dict(valid)
                state[field] = changed
                with self.subTest(field=field):
                    errors = gate_errors(state)
                    self.assertTrue(errors)
                    self.assertIn(field, "\n".join(errors))

    def test_screenshot_requires_post_request_identity_and_two_stable_sizes(self):
        identity = _required_api("capture_file_identity")
        errors = _required_api("screenshot_observation_errors")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = os.path.join(temp_dir, "frame.png")
            initial = identity(path)
            request_time_ns = 1
            with open(path, "wb") as stream:
                stream.write(b"png-bytes")
            first = identity(path)
            second = identity(path)

            self.assertIsNone(initial)
            self.assertEqual(
                errors(initial, first, second, request_time_ns), []
            )
            unstable = dict(second)
            unstable["size"] += 1
            self.assertIn(
                "stable size",
                "\n".join(errors(initial, first, unstable, request_time_ns)),
            )
            self.assertIn(
                "initially absent",
                "\n".join(errors(first, first, second, request_time_ns)),
            )

    def test_final_capture_recheck_rejects_a_replaced_early_frame(self):
        recheck = _required_api("cf1_capture_evidence_recheck_errors")
        identity = _required_api("capture_file_identity")
        with tempfile.TemporaryDirectory() as temp_dir:
            path = os.path.join(temp_dir, "MainMap_CF1_A_PRE_0900.png")
            with open(path, "wb") as stream:
                stream.write(b"accepted frame")
            accepted = identity(path)
            evidence = {
                "A_PRE_0900": {
                    "path": accepted["path"],
                    "final_mtime_ns": accepted["mtime_ns"],
                    "size": accepted["size"],
                    "sha256": accepted["sha256"],
                }
            }
            sequence = (("A_PRE", "0900"),)

            self.assertEqual(recheck(sequence, evidence), [])
            with open(path, "wb") as stream:
                stream.write(b"replaced after acceptance")
            self.assertTrue(recheck(sequence, evidence))

    def test_loaded_scene_hash_is_canonical_and_rejects_spawn_actor_paths(self):
        canonical = _required_api("canonical_loaded_scene")
        scene_hash = _required_api("loaded_scene_sha256")
        scene = self._loaded_scene()
        canonical_text = canonical(scene).decode("utf-8")
        self.assertNotIn("actor_path", canonical_text)
        self.assertNotIn("-0.0", canonical_text)
        self.assertEqual(len(scene_hash(scene)), 64)
        drifted_order = dict(scene)
        drifted_order["city_buildings"] = list(reversed(scene["city_buildings"]))
        self.assertEqual(scene_hash(drifted_order), scene_hash(scene))
        contaminated = dict(scene)
        contaminated["city_buildings"] = [dict(scene["city_buildings"][0])]
        contaminated["city_buildings"][0]["actor_path"] = (
            "PersistentLevel.BP_MB001_C_214748"
        )
        with self.assertRaises(ValueError):
            canonical(contaminated)

    def test_load_gate_reports_named_plot_or_building_projection_drift(self):
        gate_errors = _required_api("cf1_load_gate_errors")
        expected = self._loaded_scene()
        runtime = json.loads(json.dumps(expected))
        self.assertEqual(gate_errors(expected, runtime), [])
        runtime["plots"][0]["owned"] = False
        self.assertIn("OwnedPlotIds", "\n".join(gate_errors(expected, runtime)))
        runtime = json.loads(json.dumps(expected))
        runtime["placed_buildings"][0]["applied_skin_id"] = 101
        self.assertIn("Buildings", "\n".join(gate_errors(expected, runtime)))

    def test_load_gate_rejects_duplicate_plot_and_building_ids_on_both_sides(self):
        gate_errors = _required_api("cf1_load_gate_errors")
        for side in ("slot", "runtime"):
            for collection, expected_text in (
                ("plots", "duplicate plot_id"),
                ("placed_buildings", "duplicate building_index"),
            ):
                expected = self._loaded_scene()
                runtime = json.loads(json.dumps(expected))
                target = expected if side == "slot" else runtime
                target[collection].append(
                    json.loads(json.dumps(target[collection][0]))
                )
                with self.subTest(side=side, collection=collection):
                    self.assertIn(
                        expected_text,
                        "\n".join(gate_errors(expected, runtime)),
                    )

    def test_active_triplet_allows_only_the_candidate_midtone_value(self):
        triplet_errors = _required_api("cf1_active_triplet_errors")
        a_pre = self._active_readback((1.0, 1.0, 1.0, 1.0))
        candidate = self._active_readback((1.15, 1.15, 1.15, 1.0))
        a_post = self._active_readback((1.0, 1.0, 1.0, 1.0))
        self.assertEqual(triplet_errors(a_pre, candidate, a_post), [])
        candidate["exposure"]["bias"] = 0.21
        self.assertIn(
            "exposure.bias",
            "\n".join(triplet_errors(a_pre, candidate, a_post)),
        )

    def test_active_triplet_malformed_values_return_errors_not_exceptions(self):
        triplet_errors = _required_api("cf1_active_triplet_errors")
        a_pre = self._active_readback((1.0, 1.0, 1.0, 1.0))
        candidate = self._active_readback((1.15, 1.15, 1.15, 1.0))
        a_post = self._active_readback((1.0, 1.0, 1.0, 1.0))
        for malformed in (
            None,
            [],
            {"grading": []},
            {"grading": {"color_saturation_midtones": {"value": [1, float("nan")]}}},
        ):
            with self.subTest(malformed=repr(malformed)):
                errors = triplet_errors(a_pre, malformed, a_post)
                self.assertTrue(errors)


if __name__ == "__main__":
    unittest.main()
