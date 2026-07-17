#!/usr/bin/env python3
"""Perceptual image comparison for the golden-render harness.

    compare.py <ref.png> <test.png> [--profile strict|cross|loose] [--json]

Metrics: PSNR (dB), SSIM (gaussian 11x11 sigma=1.5, standard Wang et al.
constants), mean absolute diff and max absolute diff (8-bit scale).

Profiles (thresholds from the 2026-07 measurement session — same-machine
llvmpipe renders are bit-stable in practice, GL vs Vulkan frame means matched
to 4 decimal digits):
  strict : same backend-class regression gate.
           PSNR >= 40 dB  AND  SSIM >= 0.995  AND  max_abs <= 8
  cross  : GL vs Vulkan / driver-vs-driver informational check.
           PSNR >= 30 dB  AND  SSIM >= 0.99
  loose  : structural sanity only.  SSIM >= 0.95
  self   : ref-acceptance self-consistency (two renders of one case).
           PSNR >= 45 dB  AND  max_abs <= 4

Exit codes: 0 pass, 1 fail, 2 usage/IO error.  Identical files short-circuit
to PASS with psnr=inf.  A size mismatch is always FAIL (never resampled:
resolution drift IS a regression).
"""
import argparse
import json
import sys

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

PROFILES = {
    "strict": dict(psnr=40.0, ssim=0.995, max_abs=8, mean_abs=None),
    "cross": dict(psnr=30.0, ssim=0.99, max_abs=None, mean_abs=None),
    "loose": dict(psnr=None, ssim=0.95, max_abs=None, mean_abs=None),
    "self": dict(psnr=45.0, ssim=None, max_abs=4, mean_abs=None),
}


def load(path):
    try:
        img = Image.open(path).convert("RGB")
    except Exception as e:  # noqa: BLE001
        print(f"ERROR: cannot read {path}: {e}", file=sys.stderr)
        sys.exit(2)
    return np.asarray(img, dtype=np.float64)


def psnr(a, b):
    mse = np.mean((a - b) ** 2)
    if mse == 0:
        return float("inf")
    return 10.0 * np.log10(255.0**2 / mse)


def ssim(a, b, sigma=1.5):
    """Mean SSIM over the luma plane, gaussian-windowed (Wang et al. 2004)."""
    # ITU-R BT.601 luma; SSIM on luma is the standard single-channel variant.
    la = a @ np.array([0.299, 0.587, 0.114])
    lb = b @ np.array([0.299, 0.587, 0.114])
    c1, c2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    mu_a = gaussian_filter(la, sigma)
    mu_b = gaussian_filter(lb, sigma)
    mu_a2, mu_b2, mu_ab = mu_a * mu_a, mu_b * mu_b, mu_a * mu_b
    var_a = gaussian_filter(la * la, sigma) - mu_a2
    var_b = gaussian_filter(lb * lb, sigma) - mu_b2
    cov = gaussian_filter(la * lb, sigma) - mu_ab
    num = (2 * mu_ab + c1) * (2 * cov + c2)
    den = (mu_a2 + mu_b2 + c1) * (var_a + var_b + c2)
    return float(np.mean(num / den))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref")
    ap.add_argument("test")
    ap.add_argument("--profile", choices=PROFILES, default="strict")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()

    a, b = load(args.ref), load(args.test)
    out = {"ref": args.ref, "test": args.test, "profile": args.profile}

    if a.shape != b.shape:
        out.update(verdict="FAIL", reason=f"size mismatch {a.shape} vs {b.shape}")
        print(json.dumps(out) if args.json else f"FAIL size-mismatch {a.shape} vs {b.shape}")
        sys.exit(1)

    diff = np.abs(a - b)
    m = {
        "psnr": round(psnr(a, b), 3),
        "ssim": round(ssim(a, b), 6),
        "mean_abs": round(float(diff.mean()), 4),
        "max_abs": float(diff.max()),
    }
    out.update(m)

    th = PROFILES[args.profile]
    fails = []
    if th["psnr"] is not None and m["psnr"] < th["psnr"]:
        fails.append(f"psnr {m['psnr']} < {th['psnr']}")
    if th["ssim"] is not None and m["ssim"] < th["ssim"]:
        fails.append(f"ssim {m['ssim']} < {th['ssim']}")
    if th["max_abs"] is not None and m["max_abs"] > th["max_abs"]:
        fails.append(f"max_abs {m['max_abs']} > {th['max_abs']}")

    out["verdict"] = "FAIL" if fails else "PASS"
    if fails:
        out["reason"] = "; ".join(fails)

    if args.json:
        print(json.dumps(out))
    else:
        line = f"{out['verdict']} psnr={m['psnr']} ssim={m['ssim']} mean_abs={m['mean_abs']} max_abs={m['max_abs']}"
        if fails:
            line += f"  [{out['reason']}]"
        print(line)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
