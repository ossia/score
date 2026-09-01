#!/usr/bin/env python3
"""Intent assertions for golden-render output.

    assert-content.py <refs-dir> [--case NAME ...]

A reference image only pins what the renderer *did*. It cannot tell you the
renderer did the *right* thing: a backend that quietly falls back and paints a
constant colour produces a perfectly stable, perfectly reproducible reference.
That has already happened on this bench -- an offscreen run lands on the Null
RHI backend, logs "This output will never render", and still writes a solid
yellow 1280x720 PNG that passes both a blankness check and a two-run
self-consistency check.

So each case below is checked against what its own shader header says it must
look like, computed from the shader's formula where the header gives one. These
assertions are reference-free: they hold on every backend, and on a brand-new
one that has no references yet.

Cases with no entry here are reported as UNCHECKED, not as passing.
"""
import sys
import pathlib
import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)


def _fail(msg):
    return (False, msg)


def _ok(msg):
    return (True, msg)


# --- individual case assertions ---------------------------------------------

def solid_color(img, _all):
    """isf-solid-color.fs: gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0) -> magenta."""
    want = np.array([255, 0, 255], dtype=np.int16)
    bad = np.any(img != want, axis=2)
    n = int(bad.sum())
    if n:
        got = img[np.unravel_index(int(np.argmax(bad)), bad.shape)]
        return _fail(f"{n} px != magenta; first offender {tuple(int(v) for v in got)}")
    return _ok("every pixel is exactly (255,0,255)")


def gradient_2d(img, _all):
    """2d-no-stride.cs: R = x/(w-1), G = y/(h-1), B = 0.

    The header calls for black top-left, red top-right, green bottom-left,
    yellow bottom-right. Red-vs-x is checked in the stated direction; green-vs-y
    is accepted in EITHER direction but the direction is reported, because as
    measured on 2026-08-21 every backend puts green at the TOP -- i.e. the
    compute-written image reaches the window vertically flipped with respect to
    the shader's own documented orientation. Pinning the flip as "correct" would
    freeze a convention nobody has ruled on; ignoring it would lose the fact.
    """
    h, w, _ = img.shape
    r, g, b = img[:, :, 0], img[:, :, 1], img[:, :, 2]
    if int(b.max()) > 8:
        return _fail(f"blue channel should be 0, max is {int(b.max())}")
    rc = r.mean(axis=0)
    gr = g.mean(axis=1)

    # Monotonicity has to be judged against the ramp's OWN step size. Over 720
    # rows a full 0..255 ramp advances ~0.35 per row, so a fixed +-1.0 per-step
    # tolerance accepts a perfectly reversed ramp -- which is how an earlier
    # version of this check reported "as documented" on a flipped image.
    def direction(prof, axis_len, what):
        span = float(prof[-1] - prof[0])
        if abs(span) < 235:
            return None, f"{what} does not span 0..255 ({prof[0]:.1f}..{prof[-1]:.1f})"
        tol = abs(span) / axis_len * 0.25
        d = np.diff(prof) * (1 if span > 0 else -1)
        if np.any(d < -tol):
            return None, f"{what} is not monotonic"
        return (span > 0), None

    up, err = direction(rc, len(rc), "red across x")
    if err:
        return _fail(err)
    if not up:
        return _fail("red decreases left-to-right; the header calls for red at top-RIGHT")
    up, err = direction(gr, len(gr), "green across y")
    if err:
        return _fail(err)
    # The header rules: 2d-no-stride.cs writes v = pos.y/(size.y-1) and documents
    # "green (bottom-left) to yellow (bottom-right)", so green increases DOWNWARD.
    # This used to report the flip and pass anyway, on the grounds that no one had
    # ruled on the convention. The repo has since ruled -- GfxOrientationFindings
    # files the raw-imageStore flip as a defect, GfxCsfOrientMacros states that a
    # generator written through IMG_STORE lands the right way up, and GfxMrtPattern
    # and GfxWindowPattern pin the same convention analytically on four backends.
    # Abstaining here is what let two reference sets store a picture and its mirror
    # image and call both correct.
    if not up:
        # The header rules: 2d-no-stride.cs writes v = pos.y/(size.y-1) and
        # documents "green (bottom-left) to yellow (bottom-right)", so green
        # increases DOWNWARD. This used to detect the flip, print it, and return
        # OK anyway, on the grounds that no one had ruled on the convention --
        # which is how two reference sets came to hold a picture and its mirror
        # image and have both blessed.
        return _fail("green increases upward: vertically flipped against the "
                     "shader header, which documents green at the bottom. A "
                     "compute shader must store through IMG_STORE, not raw "
                     "imageStore, to get libisf's origin correction on OpenGL")
    orient = "green bottom (as documented)"
    dark = int((img[h // 8:, w // 8:].sum(axis=2) < 24).sum())
    if dark:
        return _fail(f"{dark} black pixels outside the black corner")
    return _ok(f"gradient monotone in both axes, blue flat, {orient}")


def stride_matches_baseline(img, allrefs):
    """2d-stride-xy.cs header: "Should display identical gradient to
    2d-no-stride baseline." A strided dispatch that drops threads shows a grid
    of black patches, so the two must be pixel-identical."""
    base = allrefs.get("build-2d-no-stride")
    if base is None:
        return _fail("baseline build-2d-no-stride is missing, cannot compare")
    if base.shape != img.shape:
        return _fail(f"size differs from baseline {base.shape} vs {img.shape}")
    d = np.abs(base - img)
    n = int((d.max(axis=2) > 0).sum())
    if n:
        return _fail(f"{n} px differ from the no-stride baseline (max {int(d.max())})")
    return _ok("pixel-identical to the no-stride baseline")


def two_images(img, _all):
    """isf-two-images.fs blends imageA and imageB across a smoothstep split.

    Its header promises a split screen of two different pictures, but
    common.js's buildWithTwoImages() feeds BOTH inlets from addImageProcess()
    with the same default TEST_IMAGE, so the two halves are the same picture and
    "the halves must differ" is not a property this case has. What it can still
    prove is the failure its header names -- "if only one binds: one half
    black" -- so that is what is asserted.
    """
    w = img.shape[1]
    lm = float(img[:, : w // 3].mean())
    rm = float(img[:, -(w // 3):].mean())
    if lm < 4:
        return _fail(f"left third is black (mean {lm:.2f}) - imageA did not bind")
    if rm < 4:
        return _fail(f"right third is black (mean {rm:.2f}) - imageB did not bind")
    if len(np.unique(img.reshape(-1, 3), axis=0)) < 3:
        return _fail("flat fill, not a render")
    return _ok(f"both inlets bound (third-means {lm:.2f}/{rm:.2f})")


def nearest_filter(img, _all):
    """isf-nearest-filter.fs: "If NEAREST works, you see sharp blocky pixels.
    If it falls back to LINEAR, edges are blurry."

    The step is measured PER CHANNEL. A luma or channel-sum metric silently
    reports zero here: the checkerboard's two colours are (255,51,0) and
    (0,51,255), whose sums are both 306, so the real full-scale edge cancels.
    """
    d = np.abs(np.diff(img.astype(np.int32), axis=1))
    step = int(d.max())
    if step < 120:
        return _fail(f"no hard edge found (max per-channel step {step}) - looks filtered")
    return _ok(f"hard edges present (max per-channel step {step})")


def not_flat(img, _all):
    """Multi-output / multi-pass cases: whatever they draw, a single flat
    colour means the pipeline collapsed. This is the weak fallback assertion --
    it is here to catch the Null-backend constant fill, not to validate content.
    """
    colors = len(np.unique(img.reshape(-1, 3), axis=0))
    if colors < 3:
        return _fail(f"only {colors} distinct colour(s) - flat fill, not a render")
    return _ok(f"{colors} distinct colours")


def flat_exact(want, why):
    """A case whose correct output is a single known colour."""
    want = np.array(want, dtype=np.int16)

    def check(img, _all):
        colors = np.unique(img.reshape(-1, 3), axis=0)
        if len(colors) != 1:
            return _fail(f"{len(colors)} colours, expected the single {tuple(int(v) for v in want)}")
        got = colors[0]
        if int(np.abs(got - want).max()) > 1:
            return _fail(f"flat {tuple(int(v) for v in got)}, expected {tuple(int(v) for v in want)}")
        return _ok(f"flat {tuple(int(v) for v in got)} - {why}")

    return check


CHECKS = {
    "build-isf-solid-color": solid_color,
    "build-2d-no-stride": gradient_2d,
    "build-2d-stride-xy": stride_matches_baseline,
    "build-isf-two-images": two_images,
    "build-isf-nearest-filter": nearest_filter,
    # Weak-but-real: these have no closed-form expectation in their headers.
    "build-isf-image-passthrough": not_flat,
    "build-isf-three-pass": not_flat,
    "build-isf-multipass-size": not_flat,
    "build-isf-mrt-four-outputs": not_flat,
    "build-mrt-gbuffer": not_flat,
    "build-pass-override-state": not_flat,
    "build-isf-pass-format-rgba16f": not_flat,
    # OUTPUTS FORMAT rgba16f. Its own header concedes the window "may still
    # display clamped", and it does: a flat white frame. So this case cannot
    # distinguish FORMAT honoured from FORMAT ignored -- both clamp to white.
    # All it can catch is a backend painting something else entirely, which is
    # exactly the Null-backend yellow. Worth keeping, worth not overclaiming.
    "build-output-format-rgba16f": flat_exact((255, 255, 255), "clamped HDR white (see note)"),
    "build-csf-texture-sampling": not_flat,
    # point3D "dir" DEFAULT [0.2, 0.6, 0.9] -> (51, 153, 229). The header says
    # this case must render "a flat colour sourced from the point3d port", so
    # the default is the answer and the whole frame can be checked against it.
    "build-isf-point3d-as-color": flat_exact((51, 153, 229), "the point3D DEFAULT [0.2,0.6,0.9]"),
}


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    refs = pathlib.Path(args[0])
    only = args[2:] if len(args) > 2 and args[1] == "--case" else None

    pngs = sorted(refs.glob("*.png"))
    if not pngs:
        print(f"no images in {refs}")
        return 2
    allrefs = {p.stem: load(p) for p in pngs}

    ok = bad = unchecked = 0
    for name in sorted(allrefs):
        if only and name not in only:
            continue
        fn = CHECKS.get(name)
        print(f"{name:<42}", end="")
        if fn is None:
            print("UNCHECKED (no intent assertion)")
            unchecked += 1
            continue
        good, msg = fn(allrefs[name], allrefs)
        print(("OK   " if good else "FAIL ") + msg)
        ok, bad = (ok + 1, bad) if good else (ok, bad + 1)

    print("----")
    print(f"intent[{refs.name}]: {ok} ok, {bad} failing, {unchecked} unchecked")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
