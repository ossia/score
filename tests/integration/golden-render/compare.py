#!/usr/bin/env python3
"""The golden-image comparator. One golden per case, every backend.

    compare.py <golden.png> <actual.png> [--profile shared|self] [--json]
               [--diff-dir DIR] [--name NAME]

THIS FILE IS THE ONLY PLACE A GOLDEN TOLERANCE IS DEFINED. golden-render.sh,
text-render.sh and the C++ GoldenImage.hpp helper all route through it, so a
threshold changed here changes everywhere and nowhere else. Do not reimplement
the metrics in a caller; call this.

-----------------------------------------------------------------------------
WHY THE GOLDENS ARE NOT PER-BACKEND
-----------------------------------------------------------------------------
They used to be: refs/{llvmpipe,nvidia,nvidia-vulkan,vulkan-lavapipe}/<case>.png,
four blessed copies of the same picture. That arrangement cannot condemn a
regression, only describe one. A golden blessed on llvmpipe says nothing about
what NVIDIA renders, so a change that breaks NVIDIA and only NVIDIA is compared
against a reference that moved with it, and the table stays green.

The four ref sets were measured against each other before they were collapsed
(15 cases x 6 backend pairs = 90 comparisons, Mesa llvmpipe GL 25.2.8, NVIDIA
Quadro RTX 4000 GL 595.84, the same Quadro on Vulkan, and Mesa lavapipe Vulkan):

    worst PSNR over all 90 pairs      52.61 dB
    worst SSIM                        0.99924
    worst max per-pixel abs diff      2 codes  (out of 255)
    pixels differing by more than 2   0.000 %

A software rasteriser and a discrete NVIDIA GPU, across two graphics APIs,
agree to within two code values and never disagree by more than that on a
single pixel. The per-backend split was not encoding a real difference. One
golden per case is therefore not a compromise: it is what the pixels already
said.

-----------------------------------------------------------------------------
WHY NOT A PSNR FLOOR ALONE
-----------------------------------------------------------------------------
PSNR was asked for, and PSNR is here, but PSNR alone will not hold this bar.
It is a single scalar averaged over the whole frame, so its sensitivity to a
defect falls off with the defect's area. Measured on build-2d-no-stride
(1280x720), inverting a block of the golden and scoring the result:

    inverted 4x4 block      16 px    0.002 % of frame    47.61 dB
    inverted 8x8 block      64 px    0.007 %             41.61 dB
    inverted 16x16 block   256 px    0.028 %             35.64 dB
    real llvmpipe vs nvidia disagreement                 62.39 dB (max_abs 1)

An 8x8 block of completely wrong pixels scores 41.61 dB. The floor this file
used to carry was 40 dB, so a fully corrupt block passed the gate. Meanwhile
the honest cross-vendor disagreement sits at 62 dB. There is no single PSNR
number that admits the second and rejects the first: they are only 20 dB apart
and the ordering is not even reliable, because a large-area 1-code dither can
score worse than a small-area total corruption.

So the gate is three axes, each catching what the others average away:

  psnr      >= 40 dB   Global fidelity. Catches diffuse, whole-frame drift --
                       a gamma change, a colour-space slip, a precision loss --
                       where no single pixel is far off but everything moved.
                       A uniform +3 shift on every pixel scores 38.59 dB and
                       is rejected here; max_abs would have waved it through.

  max_abs   <= 8       Localised catastrophe. This is the axis that condemns
                       the inverted block: 255 > 8, regardless of how few
                       pixels are involved and how little PSNR noticed.
                       Observed cross-backend worst case is 2, so this is 4x
                       the real envelope.

  frac(>2)  <= 0.1 %   The release valve that makes max_abs survivable. A
                       handful of outlier pixels -- an antialiased edge, a
                       rasterisation tie broken the other way -- must not fail
                       a run, but a *region* of them must. Observed
                       cross-backend value is 0.000 %, so any measurable
                       population of differing pixels is already anomalous.

  ssim      >= 0.99    Structure, retained from the previous model. Catches a
                       shift or a warp that preserves the histogram, which all
                       three axes above can miss on flat content.

Every threshold above is stated against a measured number, not chosen for
roundness. The margins are deliberately wide on the axes where an unseen
vendor could reasonably differ (max_abs, ssim) and tight on the axis that only
moves when something is actually wrong (frac).

Exit codes: 0 pass, 1 fail, 2 usage/IO error. Identical files short-circuit to
PASS with psnr=inf. A size mismatch is always FAIL (never resampled:
resolution drift IS a regression).
"""
import argparse
import json
import os
import sys

import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter

# The single source of truth. See the module docstring for the measurement
# behind every number.
PROFILES = {
    # The gate. One golden, any backend, any vendor, any OS.
    "shared": dict(psnr=40.0, ssim=0.99, max_abs=8, pixel_tol=2, max_frac=0.001),
    # Ref acceptance: two renders of one case on one machine must agree far
    # more tightly than two backends need to, or the render is not stable
    # enough to be a reference at all.
    "self": dict(psnr=45.0, ssim=None, max_abs=4, pixel_tol=1, max_frac=0.001),
}
DEFAULT_PROFILE = "shared"


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


def write_artifacts(outdir, name, golden, actual, per_pixel, pixel_tol):
    """Write golden, actual and a diff next to each other.

    A CI failure that only prints numbers cannot be diagnosed without a local
    reproduction, and the machine that produced it is usually not available.
    Three PNGs and the metric line are enough to tell a real regression from a
    driver difference without rerunning anything.

    The diff is not a subtraction: a raw difference of two nearly-identical
    frames is a black rectangle. Pixels within tolerance are shown as the
    dimmed golden so the picture stays recognisable, and offending pixels are
    painted on a red->yellow->white ramp by severity, so the eye lands on the
    defect and its shape is readable (a block, an edge, a whole surface).
    """
    try:
        os.makedirs(outdir, exist_ok=True)
        Image.fromarray(golden.astype(np.uint8)).save(
            os.path.join(outdir, f"{name}.golden.png"))
        Image.fromarray(actual.astype(np.uint8)).save(
            os.path.join(outdir, f"{name}.actual.png"))

        base = (golden * 0.25).astype(np.uint8)
        over = per_pixel > pixel_tol
        # severity in [0,1] over the offending range
        sev = np.zeros_like(per_pixel)
        if over.any():
            hi = max(float(per_pixel.max()), pixel_tol + 1.0)
            sev[over] = (per_pixel[over] - pixel_tol) / (hi - pixel_tol)
        r = np.clip(128 + 127 * sev, 0, 255)
        g = np.clip(255 * np.clip(sev * 2 - 0.5, 0, 1), 0, 255)
        b = np.clip(255 * np.clip(sev * 2 - 1.5, 0, 1), 0, 255)
        heat = np.dstack([r, g, b]).astype(np.uint8)
        out = base.copy()
        out[over] = heat[over]
        Image.fromarray(out).save(os.path.join(outdir, f"{name}.diff.png"))
        return os.path.join(outdir, f"{name}.diff.png")
    except Exception as e:  # noqa: BLE001
        print(f"WARNING: could not write diff artifacts: {e}", file=sys.stderr)
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ref", help="the golden")
    ap.add_argument("test", help="the render under test")
    ap.add_argument("--profile", choices=PROFILES, default=DEFAULT_PROFILE)
    ap.add_argument("--json", action="store_true")
    ap.add_argument(
        "--diff-dir",
        help="on FAIL, write <name>.golden.png, <name>.actual.png and "
             "<name>.diff.png here")
    ap.add_argument(
        "--name",
        help="basename for the artifacts (defaults to the golden's basename)")
    args = ap.parse_args()

    name = args.name or os.path.splitext(os.path.basename(args.ref))[0]
    a, b = load(args.ref), load(args.test)
    out = {"ref": args.ref, "test": args.test, "profile": args.profile,
           "name": name}

    if a.shape != b.shape:
        out.update(verdict="FAIL", reason=f"size mismatch {a.shape} vs {b.shape}")
        print(json.dumps(out) if args.json
              else f"FAIL size-mismatch {a.shape} vs {b.shape}")
        sys.exit(1)

    th = PROFILES[args.profile]
    diff = np.abs(a - b)
    # Worst channel error at each pixel: a pixel is "differing" if ANY of its
    # channels moved, not if the average of the three did.
    per_pixel = diff.max(axis=2)
    frac = float((per_pixel > th["pixel_tol"]).sum()) / per_pixel.size

    m = {
        "psnr": round(psnr(a, b), 3),
        "ssim": round(ssim(a, b), 6),
        "mean_abs": round(float(diff.mean()), 4),
        "max_abs": float(diff.max()),
        "frac_over": round(frac, 8),
        "pixels_over": int((per_pixel > th["pixel_tol"]).sum()),
        "pixel_tol": th["pixel_tol"],
    }
    out.update(m)

    fails = []
    if th["psnr"] is not None and m["psnr"] < th["psnr"]:
        fails.append(f"psnr {m['psnr']} < {th['psnr']}")
    if th["ssim"] is not None and m["ssim"] < th["ssim"]:
        fails.append(f"ssim {m['ssim']} < {th['ssim']}")
    if th["max_abs"] is not None and m["max_abs"] > th["max_abs"]:
        fails.append(f"max_abs {m['max_abs']} > {th['max_abs']}")
    if th["max_frac"] is not None and frac > th["max_frac"]:
        fails.append(
            f"frac_over({th['pixel_tol']}) {100*frac:.4f}% > "
            f"{100*th['max_frac']:.4f}% ({m['pixels_over']} px)")

    out["verdict"] = "FAIL" if fails else "PASS"
    if fails:
        out["reason"] = "; ".join(fails)
        if args.diff_dir:
            d = write_artifacts(args.diff_dir, name, a, b, per_pixel,
                                th["pixel_tol"])
            if d:
                out["diff"] = d

    if args.json:
        print(json.dumps(out))
    else:
        line = (f"{out['verdict']} psnr={m['psnr']} ssim={m['ssim']} "
                f"mean_abs={m['mean_abs']} max_abs={m['max_abs']} "
                f"over{th['pixel_tol']}={100*frac:.4f}%")
        if fails:
            line += f"  [{out['reason']}]"
            if out.get("diff"):
                line += f"  diff={out['diff']}"
        print(line)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
