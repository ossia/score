#!/usr/bin/env python3
"""Pixel-value assertions for the TEXT node render sweep.

    analyze.py <grab-dir>

<grab-dir> holds one 1280x720 PNG per case, produced by text-render.sh from
a single live app run. Background is pure black; text is drawn white (or the
case's color) so "text pixels" = pixels with luma > LUMA_T.

Assertions are VALUES, not just non-blankness:
  default     untouched controls MUST render visible text (guards the fix for
              the off-screen-default bug; see the check body for file:line)
  default-pos0 position (0,0) with default string/font/size: MUST be visible
  base        solid coverage; reference bbox/centroid for the movement cases
  sizes       coverage(96pt) > coverage(48pt) > coverage(24pt), bbox grows
  font        DejaVu Sans Mono vs Noto Sans rasterize differently
  color       red case: R channel dominates G/B on text pixels
  position    pos-right centroid right of pos-left; pos-down moves centroid
              DOWN in image coords (clip -y = screen down); y-cases share x
  scale       0.5 scale halves the bbox (within tolerance) + less coverage
  unicode/cjk render with real coverage (accented Latin + CJK glyphs)
  longstr     ~2000 chars: more coverage than base, no crash
  tofu        unassigned codepoints: no crash; frame present (tofu boxes or
              blank both acceptable)
  empty       clean blank frame
  base-again  bit-near-identical to base (recovery after edge cases +
              in-run determinism)

Every one of those is a statement about a picture, and none of them said the
picture had to BE one. Graded against sixteen identical solid-yellow
rectangles -- which is exactly what this suite was fed for its whole life, the
Qt::yellow the QRhi Null backend fills a texture with when no GL context could
be created -- 27 of the 36 assertions below passed. A uniform frame has a
centroid (the middle), a bbox (the frame), and a coverage of 921600, so
"visible", "coverage > 500", "centroid unmoved", "recovers to base" and every
grab-exists check are all satisfied by a rectangle of one colour.

So each frame is first held to a structural contract, and a frame that fails it
is treated as a failed grab for every assertion downstream:

  * it must contain background. The scene clears to pure black, so a real frame
    has near-black pixels in it; measured minimum luma is 0.0 on all seventeen.
    A solid fill of ANY non-black colour dies here, and a legitimately blank
    frame (empty string) is uniformly black and survives, which is the one
    uniform frame that is allowed.
  * text cannot fill the frame. Measured worst case is size-large at 1.35 %
    coverage; the ceiling is 25 %.

And no assertion may vanish: guards that used to skip a check when their inputs
were missing now fail it, because a check that disappears is one nobody counts.
The suite reports 36 checks whatever happens.

Exit 0 iff all assertions pass; prints one line per check.
"""
import sys
import pathlib

import numpy as np
from PIL import Image

LUMA_T = 40.0  # text-pixel threshold on BT.601 luma (bg is pure black)
# Structural contract, both sides measured on a real llvmpipe sweep (see the
# module docstring): every frame's darkest pixel is at luma 0.0, and the
# heaviest case covers 1.35 % of the frame.
BG_LUMA_MAX = 8.0
MAX_COVER = 0.25

failures = []
checks = 0


def check(name, cond, detail=""):
    global checks
    checks += 1
    status = "ok  " if cond else "FAIL"
    print(f"  [{status}] {name} {detail}")
    if not cond:
        failures.append(name)


def load(d, name):
    p = d / f"{name}.png"
    if not p.is_file():
        return None
    return np.asarray(Image.open(p).convert("RGB"), dtype=np.float64)


def luma(img):
    return img @ np.array([0.299, 0.587, 0.114])


def stats(img):
    """(count, bbox(x0,y0,x1,y1), centroid(cx,cy)) of text pixels."""
    mask = luma(img) > LUMA_T
    n = int(mask.sum())
    if n == 0:
        return 0, None, None
    ys, xs = np.nonzero(mask)
    return n, (xs.min(), ys.min(), xs.max(), ys.max()), (xs.mean(), ys.mean())


def frame_defect(img):
    """Why this cannot be a frame of text on black, or None if it could be."""
    if img is None:
        return "no grab written"
    lum = luma(img)
    lo = float(lum.min())
    if lo > BG_LUMA_MAX:
        px = tuple(int(v) for v in img.reshape(-1, 3)[int(np.argmin(lum))])
        flat = "uniform " if float(lum.max()) - lo < 1e-9 else ""
        return (f"{flat}fill, darkest pixel RGB={px} luma={lo:.0f} "
                f"- the scene clears to black, so nothing rendered")
    frac = float((lum > LUMA_T).mean())
    if frac > MAX_COVER:
        return f"lit coverage {frac:.1%} > {MAX_COVER:.0%} - text cannot fill the frame"
    return None


def main():
    d = pathlib.Path(sys.argv[1])
    imgs = {}
    for name in ["default", "default-pos0", "base", "base-again", "size-small",
                 "size-large", "font-sans", "color-red", "pos-left",
                 "pos-right", "pos-down", "scale-half", "unicode", "cjk",
                 "longstr", "tofu", "empty"]:
        img = load(d, name)
        why = frame_defect(img)
        check(f"frame:{name}", why is None, why or "")
        # A frame that is not a render is not evidence for anything: drop it so
        # every assertion below reports the absence instead of measuring the
        # fill.
        imgs[name] = img if why is None else None

    def S(n):
        return stats(imgs[n]) if imgs[n] is not None else (0, None, None)

    def have(*names):
        return all(imgs[n] is not None for n in names)

    H, W = 720, 1280
    if imgs["base"] is not None:
        H, W = imgs["base"].shape[0], imgs["base"].shape[1]

    def inside(bb):
        """bbox is a strict sub-rectangle: a string does not span the frame."""
        return bb is not None and (bb[2] - bb[0]) < W - 1 and (bb[3] - bb[1]) < H - 1

    # --- default visibility (the off-screen-default regression check) -----
    # Hard assertion: an untouched Text process MUST render visible text. This
    # guards the fix for the off-screen-default bug (the UBO default position
    # was {0.5,0.5} in Gfx/Graph/TextNode.hpp, shifting the quad +0.5 clip
    # up/right so the default text landed ~180px above the visible area; now
    # {0,0} = screen centre). Regressing it fails the suite.
    n_def, bb_def, _ = S("default")
    check("default-visible", n_def > 200 and inside(bb_def),
          f"count={n_def} bbox={bb_def} (untouched Text must render on-screen)")

    # default-pos0 sets ONLY position=(0,0), keeping the default string/font/
    # size: it must be visible, proving the default text/font/size DO
    # propagate and the position default alone causes the blank frame.
    n_dp, bb_dp, _ = S("default-pos0")
    check("default-pos0-visible", n_dp > 200 and inside(bb_dp),
          f"count={n_dp} bbox={bb_dp} (default text, repositioned on screen)")

    # --- base reference ---------------------------------------------------
    n_base, bb_base, c_base = S("base")
    check("base-coverage", n_base > 500 and inside(bb_base),
          f"count={n_base} bbox={bb_base}")

    # --- point size ordering ---------------------------------------------
    n_s, bb_s, _ = S("size-small")
    n_l, bb_l, _ = S("size-large")
    check("size-count-order", n_s > 0 and n_l > n_base > n_s,
          f"24pt={n_s} 48pt={n_base} 96pt={n_l}")
    if bb_s and bb_l:
        w_s, w_l = bb_s[2] - bb_s[0], bb_l[2] - bb_l[0]
        h_s, h_l = bb_s[3] - bb_s[1], bb_l[3] - bb_l[1]
        check("size-bbox-grows", w_l > w_s * 1.5 and h_l > h_s * 1.5,
              f"w:{w_s}->{w_l} h:{h_s}->{h_l}")
    else:
        check("size-bbox-grows", False, "missing bbox")

    # --- font family actually changes the raster -------------------------
    if imgs["font-sans"] is not None and imgs["base"] is not None:
        n_f, _, _ = S("font-sans")
        diff = int((np.abs(luma(imgs["font-sans"]) - luma(imgs["base"])) > LUMA_T).sum())
        check("font-differs", n_f > 500 and diff > 200,
              f"sans-count={n_f} differing-px={diff}")
    else:
        check("font-differs", False, "missing grabs")

    # --- color control ----------------------------------------------------
    def channel_means(name):
        """(count, R, G, B) means over the solidly-lit pixels (avoid AA fringe)."""
        if imgs[name] is None:
            return 0, 0.0, 0.0, 0.0
        img = imgs[name]
        mask = img[:, :, 0] > 128
        n = int(mask.sum())
        if n == 0:
            return 0, 0.0, 0.0, 0.0
        return (n, float(img[:, :, 0][mask].mean()), float(img[:, :, 1][mask].mean()),
                float(img[:, :, 2][mask].mean()))

    n_red, mr, mg, mb = channel_means("color-red")
    check("color-red-dominant", n_red > 0 and mr > 200 and mg < 60 and mb < 60,
          f"count={n_red} RGB=({mr:.0f},{mg:.0f},{mb:.0f})")
    # The white base must NOT pass the red predicate -- and must be white, not
    # merely bright in red and green. Checking only red and green is how a
    # (255,255,0) fill was certified white on every run this suite ever made.
    n_w, wr, wg, wb = channel_means("base")
    check("color-base-is-white",
          n_w > 0 and wr > 200 and wg > 200 and wb > 200,
          f"base count={n_w} RGB=({wr:.0f},{wg:.0f},{wb:.0f})")

    # --- position control -------------------------------------------------
    n_pl, _, c_pl = S("pos-left")
    n_pr, _, c_pr = S("pos-right")
    n_pd, _, c_pd = S("pos-down")
    have_x = have("pos-left", "pos-right", "base") and c_pl and c_pr and c_base
    # clip +x = screen right: right centroid must sit ~640px right of left
    # (1.0 clip delta = full 1280px width => 0.5+0.5 => 640px), and both
    # straddle base. Allow generous slack for clipped glyph edges.
    dx = (c_pr[0] - c_pl[0]) if have_x else 0.0
    check("pos-x-order",
          have_x and c_pl[0] < c_base[0] < c_pr[0] and dx > 300,
          (f"cx left={c_pl[0]:.0f} base={c_base[0]:.0f} right={c_pr[0]:.0f} dx={dx:.0f}"
           if have_x else f"counts l/r/base={n_pl}/{n_pr}/{n_base}"))
    # "y is unchanged" is only evidence when x actually moved -- on a frame with
    # no text at all, or one filled edge to edge, every centroid is the middle
    # and this passes by saying nothing.
    check("pos-x-keeps-y",
          have_x and abs(c_pl[1] - c_base[1]) < 40 and abs(c_pr[1] - c_base[1]) < 40,
          (f"cy l/b/r={c_pl[1]:.0f}/{c_base[1]:.0f}/{c_pr[1]:.0f}"
           if have_x else f"counts l/r/base={n_pl}/{n_pr}/{n_base}"))

    have_y = have("pos-down", "base") and c_pd and c_base
    # clip -y = screen DOWN (image row index grows): centroid must move
    # down by ~0.25*720=180px; keep x roughly unchanged.
    dy = (c_pd[1] - c_base[1]) if have_y else 0.0
    check("pos-y-down", have_y and dy > 100,
          (f"cy base={c_base[1]:.0f} down={c_pd[1]:.0f} dy={dy:.0f}"
           if have_y else f"counts down/base={n_pd}/{n_base}"))
    check("pos-y-keeps-x", have_y and abs(c_pd[0] - c_base[0]) < 40,
          (f"cx base={c_base[0]:.0f} down={c_pd[0]:.0f}"
           if have_y else f"counts down/base={n_pd}/{n_base}"))

    # --- scale -------------------------------------------------------------
    n_sc, bb_sc, _ = S("scale-half")
    if bb_sc and bb_base:
        w_b, w_h = bb_base[2] - bb_base[0], bb_sc[2] - bb_sc[0]
        ratio = w_h / max(w_b, 1)
        check("scale-half-bbox", 0.3 < ratio < 0.7 and n_sc < n_base,
              f"w {w_b}->{w_h} ratio={ratio:.2f} count {n_base}->{n_sc}")
    else:
        check("scale-half-bbox", False, f"count={n_sc}")

    # --- unicode / cjk -----------------------------------------------------
    n_u, bb_u, _ = S("unicode")
    check("unicode-coverage", n_u > 500 and inside(bb_u), f"count={n_u} bbox={bb_u}")
    n_c, bb_c, _ = S("cjk")
    check("cjk-coverage", n_c > 500 and inside(bb_c), f"count={n_c} bbox={bb_c}")

    # --- very long string --------------------------------------------------
    n_lo, _, _ = S("longstr")
    check("longstr-coverage", n_lo > n_base, f"count={n_lo} (base={n_base})")

    # --- tofu: frame present, app alive (recovery asserted by base-again) --
    # "present" now means the structural contract held: tofu boxes and a blank
    # black frame are both acceptable outcomes, a yellow rectangle is not.
    n_t, _, _ = S("tofu")
    check("tofu-rendered-frame", imgs["tofu"] is not None,
          f"count={n_t} (blank or boxes both ok)")

    # --- empty string: clean blank ----------------------------------------
    n_e, _, _ = S("empty")
    check("empty-blank", imgs["empty"] is not None and n_e < 50, f"count={n_e}")

    # --- recovery + in-run determinism ------------------------------------
    if have("base-again", "base"):
        d_max = float(np.abs(imgs["base-again"] - imgs["base"]).max())
        n_a, _, _ = S("base-again")
        check("base-recovery", d_max <= 4 and n_a > 500,
              f"max_abs={d_max} count={n_a} (vs base={n_base})")
    else:
        check("base-recovery", False, "missing grabs")

    ok = not failures
    print(f"analyze: {checks - len(failures)}/{checks} ok"
          + ("" if ok else f"  FAILURES: {', '.join(failures)}"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
