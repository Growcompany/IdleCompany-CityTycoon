"""Pure image metrics for PC-reference and mobile MainMap captures.

Floating-point NumPy inputs are interpreted as linear RGB in [0, 1]. Pillow
images and integer NumPy arrays are interpreted as 8-bit sRGB captures.
"""

import numpy as np


def _crop_bounds(crop, width, height):
    if crop is None:
        return 0, 0, width, height
    if len(crop) != 4 or any(not isinstance(value, (int, np.integer)) for value in crop):
        raise ValueError("crop must contain four integer coordinates")
    left, top, right, bottom = (int(value) for value in crop)
    if left < 0 or top < 0 or right > width or bottom > height:
        raise ValueError("crop is outside the image")
    if right <= left or bottom <= top:
        raise ValueError("ROI contains no pixels")
    return left, top, right, bottom


def _srgb_to_linear(values):
    return np.where(
        values <= 0.04045,
        values / 12.92,
        ((values + 0.055) / 1.055) ** 2.4,
    )


def _linear_rgb(image, crop=None):
    if isinstance(image, np.ndarray):
        source = np.asarray(image)
        if source.ndim != 3 or source.shape[2] != 3:
            raise ValueError("expected an HxWx3 RGB image")
        left, top, right, bottom = _crop_bounds(
            crop, source.shape[1], source.shape[0]
        )
        source = source[top:bottom, left:right]
        encoded_srgb = np.issubdtype(source.dtype, np.integer)
        values = source.astype(np.float64)
        if encoded_srgb:
            values /= 255.0
    elif hasattr(image, "convert") and hasattr(image, "crop") and hasattr(image, "size"):
        width, height = image.size
        bounds = _crop_bounds(crop, width, height)
        source = image.crop(bounds) if crop is not None else image
        values = np.asarray(source.convert("RGB"), dtype=np.float64) / 255.0
        encoded_srgb = True
    else:
        raise ValueError("expected a NumPy RGB array or Pillow image")

    if values.size == 0:
        raise ValueError("ROI contains no pixels")
    if not np.isfinite(values).all() or np.any(values < 0.0) or np.any(values > 1.0):
        raise ValueError("RGB values must be finite and in [0, 1]")
    return _srgb_to_linear(values) if encoded_srgb else values


def _metrics(linear):
    luminance = (
        linear[..., 0] * 0.2126
        + linear[..., 1] * 0.7152
        + linear[..., 2] * 0.0722
    )
    horizontal = np.abs(np.diff(luminance, axis=1))
    vertical = np.abs(np.diff(luminance, axis=0))
    edge_samples = horizontal.size + vertical.size
    edge_energy = (
        (float(horizontal.sum()) + float(vertical.sum())) / edge_samples
        if edge_samples
        else 0.0
    )
    return {
        "mean_luminance": float(luminance.mean()),
        "local_contrast": float(luminance.std()),
        "edge_energy": edge_energy,
        "chroma": float((linear.max(axis=2) - linear.min(axis=2)).mean()),
        "clipped_pixel_percent": float(
            (linear.max(axis=2) >= _srgb_to_linear(np.float64(0.995))).mean()
            * 100.0
        ),
    }


def _ratio(numerator, denominator):
    if abs(denominator) <= 1.0e-12:
        return 1.0 if abs(numerator) <= 1.0e-12 else float("inf")
    return float(numerator / denominator)


def compare_to_pc(pc_image, mobile_image, crop=None) -> dict[str, float]:
    pc = _linear_rgb(pc_image, crop)
    mobile = _linear_rgb(mobile_image, crop)
    if pc.shape != mobile.shape:
        raise ValueError("PC and mobile images must have identical shapes")

    pc_metrics = _metrics(pc)
    mobile_metrics = _metrics(mobile)
    result = {}
    for name in ("mean_luminance", "local_contrast", "edge_energy", "chroma"):
        result["pc_" + name] = pc_metrics[name]
        result["mobile_" + name] = mobile_metrics[name]
        result[name + "_ratio"] = _ratio(mobile_metrics[name], pc_metrics[name])
    result["pc_clipped_pixel_percent"] = pc_metrics["clipped_pixel_percent"]
    result["mobile_clipped_pixel_percent"] = mobile_metrics[
        "clipped_pixel_percent"
    ]
    result["clipped_pixel_pp_delta"] = (
        mobile_metrics["clipped_pixel_percent"]
        - pc_metrics["clipped_pixel_percent"]
    )
    return result


def aa_drift(pre_image, post_image, crop=None) -> dict[str, float]:
    pre = _linear_rgb(pre_image, crop)
    post = _linear_rgb(post_image, crop)
    if pre.shape != post.shape:
        raise ValueError("AA drift inputs must have identical shapes")
    difference = np.abs(pre - post)
    return {
        "mean_abs_rgb": float(difference.mean()),
        "p95_abs_rgb": float(np.percentile(difference, 95)),
    }
