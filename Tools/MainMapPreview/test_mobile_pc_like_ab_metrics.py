import unittest

import numpy as np
from PIL import Image

from mobile_pc_like_ab_metrics import aa_drift, compare_to_pc


class MobilePcLikeAbMetricsTests(unittest.TestCase):
    def test_constant_brightness_ratio_and_zero_edges(self):
        pc = np.full((32, 32, 3), 0.25, dtype=np.float32)
        mobile = np.full((32, 32, 3), 0.50, dtype=np.float32)

        result = compare_to_pc(pc, mobile)

        self.assertAlmostEqual(result["mean_luminance_ratio"], 2.0)
        self.assertAlmostEqual(result["mobile_edge_energy"], 0.0)

    def test_identical_images_have_zero_aa_drift(self):
        image = np.arange(48, dtype=np.float32).reshape(4, 4, 3) / 48.0

        result = aa_drift(image, image)

        self.assertAlmostEqual(result["mean_abs_rgb"], 0.0)
        self.assertAlmostEqual(result["p95_abs_rgb"], 0.0)

    def test_compare_reports_requested_ratios_and_clip_delta(self):
        pc = np.zeros((4, 4, 3), dtype=np.float32)
        pc[:, ::2] = (0.8, 0.2, 0.1)
        mobile = np.full((4, 4, 3), 1.0, dtype=np.float32)

        result = compare_to_pc(pc, mobile)

        self.assertEqual(
            {
                "mean_luminance_ratio",
                "local_contrast_ratio",
                "edge_energy_ratio",
                "chroma_ratio",
                "clipped_pixel_pp_delta",
            }.difference(result),
            set(),
        )
        self.assertAlmostEqual(result["mobile_chroma"], 0.0)
        self.assertAlmostEqual(result["mobile_clipped_pixel_percent"], 100.0)
        self.assertAlmostEqual(result["clipped_pixel_pp_delta"], 100.0)

    def test_scaled_pattern_has_matching_contrast_edge_and_chroma_ratios(self):
        pc = np.array(
            [
                [[0.8, 0.2, 0.1], [0.1, 0.7, 0.2]],
                [[0.3, 0.1, 0.9], [0.9, 0.4, 0.2]],
            ],
            dtype=np.float32,
        )
        mobile = pc * 0.5

        result = compare_to_pc(pc, mobile)

        self.assertAlmostEqual(result["mean_luminance_ratio"], 0.5)
        self.assertAlmostEqual(result["local_contrast_ratio"], 0.5)
        self.assertAlmostEqual(result["edge_energy_ratio"], 0.5)
        self.assertAlmostEqual(result["chroma_ratio"], 0.5)

    def test_crop_excludes_pixels_outside_roi(self):
        pc = np.zeros((4, 4, 3), dtype=np.float32)
        mobile = pc.copy()
        mobile[0, 0] = 1.0

        result = compare_to_pc(pc, mobile, crop=(1, 1, 4, 4))

        self.assertAlmostEqual(result["mean_luminance_ratio"], 1.0)
        self.assertAlmostEqual(aa_drift(pc, mobile, crop=(1, 1, 4, 4))["p95_abs_rgb"], 0.0)

    def test_pillow_srgb_is_decoded_to_linear_rgb(self):
        black = Image.fromarray(np.zeros((2, 2, 3), dtype=np.uint8), mode="RGB")
        gray = Image.fromarray(np.full((2, 2, 3), 128, dtype=np.uint8), mode="RGB")

        result = compare_to_pc(black, gray)

        expected_linear = ((128.0 / 255.0 + 0.055) / 1.055) ** 2.4
        self.assertAlmostEqual(result["mobile_mean_luminance"], expected_linear)

    def test_aa_drift_reports_mean_and_p95_channel_difference(self):
        pre = np.zeros((10, 10, 3), dtype=np.float32)
        post = pre.copy()
        post[:, :, 0] = 0.25

        result = aa_drift(pre, post)

        self.assertAlmostEqual(result["mean_abs_rgb"], 0.25 / 3.0)
        self.assertAlmostEqual(result["p95_abs_rgb"], 0.25)

    def test_rejects_mismatched_shapes_empty_crop_and_non_rgb(self):
        with self.assertRaises(ValueError):
            compare_to_pc(
                np.zeros((2, 2, 3), dtype=np.float32),
                np.zeros((3, 2, 3), dtype=np.float32),
            )
        with self.assertRaises(ValueError):
            aa_drift(
                np.zeros((2, 2, 3), dtype=np.float32),
                np.zeros((2, 2, 3), dtype=np.float32),
                crop=(1, 1, 1, 2),
            )
        with self.assertRaises(ValueError):
            compare_to_pc(
                np.zeros((2, 2), dtype=np.float32),
                np.zeros((2, 2), dtype=np.float32),
            )


if __name__ == "__main__":
    unittest.main()
