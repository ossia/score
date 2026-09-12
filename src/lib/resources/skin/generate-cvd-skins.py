#!/usr/bin/env python3
"""Generate colour skins for the three kinds of dichromacy, and verify them.

score uses colour to carry meaning in several places where the shapes are
identical: port dots (audio / midi / value), cables, curve segment types, and
the warning levels. If two of those collapse to the same perceived colour the
UI stops being readable, so the point of these skins is not "pretty pastels",
it is that the colours which must differ still differ *after* the viewer's
colour vision has removed a cone class.

So this script does not pick hues by eye. It

  1. simulates each candidate palette with Brettel, Viénot & Mollon (1997),
  2. measures the CIE76 dE between every pair that must stay distinct,
  3. fails loudly if any pair falls under the threshold.

The simulation parameters are taken verbatim from libDaltonLens
(github.com/DaltonLens/libDaltonLens, public domain), which implements
Brettel 1997 directly. Brettel is used for all three deficiencies rather than
the faster Viénot 1999 simplification, because the latter is known to be
inaccurate for tritanopia.

Base hues come from the Okabe-Ito Color Universal Design palette, which was
built empirically to survive all common deficiencies, then re-assigned per
deficiency so that the *pairs score actually contrasts* are the ones placed on
that deficiency's surviving axis:

  protanopia, deuteranopia  red/green collapse -> discriminate on blue vs
                            orange/yellow, and on lightness
  tritanopia                blue/yellow collapse -> discriminate on red vs
                            cyan/green, and on lightness

Run from this directory:  python3 generate-cvd-skins.py
Then add any new file to ../score.qrc.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- colour maths

def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c < 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(v):
    v = max(0.0, min(1.0, v))
    s = v * 12.92 if v < 0.0031308 else (v ** (1 / 2.4)) * 1.055 - 0.055
    return max(0, min(255, round(s * 255)))


def hex_to_rgb(h):
    h = h.lstrip("#")
    return [int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)]


# Brettel 1997, verbatim from libDaltonLens. Each entry is two 3x3 matrices
# (one per half-space) plus the separation plane normal, all in linear RGB.
BRETTEL = {
    "protan": (
        (0.14980, 1.19548, -0.34528, 0.10764, 0.84864, 0.04372, 0.00384, -0.00540, 1.00156),
        (0.14570, 1.16172, -0.30742, 0.10816, 0.85291, 0.03892, 0.00386, -0.00524, 1.00139),
        (0.00048, 0.00393, -0.00441),
    ),
    "deutan": (
        (0.36477, 0.86381, -0.22858, 0.26294, 0.64245, 0.09462, -0.02006, 0.02728, 0.99278),
        (0.37298, 0.88166, -0.25464, 0.25954, 0.63506, 0.10540, -0.01980, 0.02784, 0.99196),
        (-0.00281, -0.00611, 0.00892),
    ),
    "tritan": (
        (1.01277, 0.13548, -0.14826, -0.01243, 0.86812, 0.14431, 0.07589, 0.80500, 0.11911),
        (0.93678, 0.18979, -0.12657, 0.06154, 0.81526, 0.12320, -0.37562, 1.12767, 0.24796),
        (0.03901, -0.02788, -0.01113),
    ),
}


def simulate(rgb, deficiency):
    """Return rgb as a dichromat of the given type would perceive it."""
    m1, m2, normal = BRETTEL[deficiency]
    lin = [srgb_to_linear(c) for c in rgb[:3]]
    m = m1 if sum(n * v for n, v in zip(normal, lin)) >= 0 else m2
    out = [
        m[0] * lin[0] + m[1] * lin[1] + m[2] * lin[2],
        m[3] * lin[0] + m[4] * lin[1] + m[5] * lin[2],
        m[6] * lin[0] + m[7] * lin[1] + m[8] * lin[2],
    ]
    return [linear_to_srgb(v) for v in out]


def to_lab(rgb):
    r, g, b = (srgb_to_linear(c) for c in rgb[:3])
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)]


def delta_e(a, b):
    la, lb = to_lab(a), to_lab(b)
    return sum((x - y) ** 2 for x, y in zip(la, lb)) ** 0.5


def relative_luminance(rgb):
    r, g, b = (srgb_to_linear(c) for c in rgb[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast_ratio(a, b):
    """WCAG contrast ratio, 1.0 identical up to 21.0 black on white."""
    la, lb = relative_luminance(a), relative_luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


# --------------------------------------------------------------- the palettes

# Okabe-Ito Color Universal Design palette.
OI = {
    "orange": "#e69f00",
    "skyblue": "#56b4e9",
    "green": "#009e73",
    "yellow": "#f0e442",
    "blue": "#0072b2",
    "vermilion": "#d55e00",
    "purple": "#cc79a7",
    "black": "#000000",
}

# A shared neutral ramp: deficiency changes hue discrimination, not lightness,
# so the greys and backgrounds are the same in all three.
NEUTRAL = dict(
    bg_dim="#101216", bg0="#181b21", bg1="#22262e", bg2="#2e333d", bg3="#3d4450",
    grey="#7d8590", fg_dim="#b9c0cb", fg="#f0f3f8",
)

# Per-deficiency accent assignment. The keys are score's semantic slots; the
# values are chosen so the must-differ pairs below land far apart *after*
# simulation. Lightness is used as a second channel wherever hue runs out.
#
# Playback matters most here. While an interval runs, score fills it with
# IntervalPlayFill (Base3) over IntervalBase (Base1) and animates a dash in
# Pulse1, switching to Pulse2 while waiting. If any of those collapse into
# each other or into the canvas, you cannot see what is playing, so they get
# their own slots and their own checks rather than being aliases of the
# port colours.
ACCENTS = {
    # Red and green collapse. Everything meaningful is placed on the
    # blue <-> orange/yellow axis, with lightness separating the rest.
    "Protanopia": dict(
        port_a=OI["blue"], port_b=OI["yellow"], port_c=OI["skyblue"],
        warn_low=OI["yellow"], warn_mid=OI["orange"], warn_high="#ffffff",
        accent=OI["skyblue"], accent2=OI["purple"],
        curve_a=OI["blue"], curve_b=OI["yellow"], curve_c=OI["orange"],
        # Playback: a dim blue body, a bright yellow fill as it plays, a
        # near-white dash on top, and a muted grey-blue while waiting.
        interval_base="#4982bf", play_fill=OI["yellow"],
        pulse_play="#fffbe0", pulse_wait="#8aa0b8",
    ),
    # Same confusion axis as protanopia, but reds keep more of their
    # luminance, so vermilion is usable where protanopia needs white.
    "Deuteranopia": dict(
        port_a=OI["blue"], port_b=OI["yellow"], port_c=OI["skyblue"],
        warn_low=OI["yellow"], warn_mid=OI["orange"], warn_high=OI["vermilion"],
        accent=OI["skyblue"], accent2=OI["purple"],
        curve_a=OI["blue"], curve_b=OI["yellow"], curve_c=OI["vermilion"],
        interval_base="#4982bf", play_fill=OI["yellow"],
        pulse_play="#fffbe0", pulse_wait="#8aa0b8",
    ),
    # Blue and yellow collapse. Discriminate on red <-> cyan/green instead,
    # and keep blues away from yellows entirely.
    "Tritanopia": dict(
        port_a=OI["vermilion"], port_b=OI["green"], port_c="#ffffff",
        warn_low=OI["green"], warn_mid=OI["purple"], warn_high=OI["vermilion"],
        accent=OI["green"], accent2=OI["purple"],
        curve_a=OI["vermilion"], curve_b=OI["green"], curve_c="#b0b8c4",
        # Blue/yellow is unusable, so playback runs on the red <-> green axis
        # with a pale dash for the moving part.
        interval_base="#258f77", play_fill=OI["vermilion"],
        pulse_play="#ffe4d6", pulse_wait="#7f8f88",
    ),
}

# Pairs score renders side by side with identical shapes, so colour is the
# only thing telling them apart. These are what the simulation has to keep
# distinguishable; everything else is decoration.
MUST_DIFFER = [
    ("port_a", "port_b"), ("port_a", "port_c"), ("port_b", "port_c"),
    ("warn_low", "warn_mid"), ("warn_mid", "warn_high"), ("warn_low", "warn_high"),
    ("curve_a", "curve_b"), ("curve_b", "curve_c"), ("curve_a", "curve_c"),
    ("accent", "accent2"),
    # Playback. These are the pairs you look at while something is running.
    ("play_fill", "interval_base"),
    ("pulse_play", "pulse_wait"),
    ("pulse_play", "play_fill"),
    ("pulse_play", "interval_base"),
    ("pulse_wait", "interval_base"),
]

# Playback colours also have to be visible against the canvas, which dE does
# not capture well for a light mark on a dark ground. WCAG contrast ratio
# against both backgrounds, checked under simulation too.
AGAINST_BACKGROUND = ["play_fill", "pulse_play", "pulse_wait", "interval_base"]
MIN_CONTRAST = 3.0

# CIE76 dE. 10 is a clear, obvious difference at these patch sizes; below
# about 5 two colours start reading as the same swatch.
THRESHOLD = 10.0

CABLE_ALPHA = 136
SELECTED_CABLE_ALPHA = 204


def build(accents):
    n = NEUTRAL
    a = accents
    return {
        "Dark": hex_to_rgb(n["bg_dim"]),
        "HalfDark": hex_to_rgb(n["bg1"]),
        "DarkGray": hex_to_rgb(n["bg2"]),
        "Gray": hex_to_rgb(n["grey"]),
        "LightGray": hex_to_rgb(n["fg_dim"]),
        "HalfLight": hex_to_rgb(n["fg_dim"]),
        "Light": hex_to_rgb(n["fg"]),

        "Emphasis1": hex_to_rgb(a["accent"]),
        "Emphasis2": hex_to_rgb(n["bg2"]),
        "Emphasis3": hex_to_rgb(a["accent2"]),
        "Emphasis4": hex_to_rgb(n["fg"]),
        "Emphasis5": hex_to_rgb(n["bg1"]),

        # Base1 is the interval body, Base2 the selected one, Base3 the fill
        # that grows across it during playback.
        "Base1": hex_to_rgb(a["interval_base"]),
        "Base2": hex_to_rgb(a["accent"]),
        "Base3": hex_to_rgb(a["play_fill"]),
        "Base4": hex_to_rgb(a["warn_mid"]),
        "Base5": hex_to_rgb(n["bg0"]),

        "Warn1": hex_to_rgb(a["warn_low"]),
        "Warn2": hex_to_rgb(a["warn_mid"]),
        "Warn3": hex_to_rgb(a["warn_high"]),

        "Background1": hex_to_rgb(n["bg0"]),
        "Background2": hex_to_rgb(n["bg1"]),

        "Transparent1": hex_to_rgb(n["bg_dim"]),
        "Transparent2": hex_to_rgb(n["grey"]),
        "Transparent3": hex_to_rgb(n["bg2"]),

        "Smooth1": hex_to_rgb(a["curve_a"]),
        "Smooth2": hex_to_rgb(a["curve_b"]),
        "Smooth3": hex_to_rgb(a["curve_c"]),

        "Tender1": hex_to_rgb(a["warn_high"]),
        "Tender2": hex_to_rgb(a["warn_low"]),
        "Tender3": hex_to_rgb(n["bg3"]),

        "Cable1": hex_to_rgb(a["port_a"]) + [CABLE_ALPHA],
        "Cable2": hex_to_rgb(a["port_b"]) + [CABLE_ALPHA],
        "Cable3": hex_to_rgb(a["port_c"]) + [CABLE_ALPHA],
        "SelectedCable1": hex_to_rgb(a["port_a"]) + [SELECTED_CABLE_ALPHA],
        "SelectedCable2": hex_to_rgb(a["port_b"]) + [SELECTED_CABLE_ALPHA],
        "SelectedCable3": hex_to_rgb(a["port_c"]) + [SELECTED_CABLE_ALPHA],

        "Port1": hex_to_rgb(a["port_a"]),
        "Port2": hex_to_rgb(a["port_b"]),
        "Port3": hex_to_rgb(a["port_c"]),

        # The animated dash: playing, then waiting.
        "Pulse1": hex_to_rgb(a["pulse_play"]),
        "Pulse2": hex_to_rgb(a["pulse_wait"]),
    }


DEFICIENCY_OF = {
    "Protanopia": "protan",
    "Deuteranopia": "deutan",
    "Tritanopia": "tritan",
}


def verify(name, accents):
    """Check the must-differ pairs under that deficiency. Returns the worst dE."""
    deficiency = DEFICIENCY_OF[name]
    worst = (None, 1e9)
    print(f"\n{name}  (simulated as {deficiency})")
    for x, y in MUST_DIFFER:
        cx, cy = hex_to_rgb(accents[x]), hex_to_rgb(accents[y])
        normal = delta_e(cx, cy)
        seen = delta_e(simulate(cx, deficiency), simulate(cy, deficiency))
        flag = "  <-- BELOW THRESHOLD" if seen < THRESHOLD else ""
        print(f"  {x:<10} vs {y:<10} dE normal {normal:6.1f}   as seen {seen:6.1f}{flag}")
        if seen < worst[1]:
            worst = ((x, y), seen)

    # Playback marks must also stand off the canvas, which a hue metric does
    # not measure. Check luminance contrast against both backgrounds.
    print("  -- contrast against the canvas --")
    worst_cr = 1e9
    for role in AGAINST_BACKGROUND:
        c = hex_to_rgb(accents[role])
        for bgname in ("bg0", "bg1"):
            bg = hex_to_rgb(NEUTRAL[bgname])
            cr = contrast_ratio(simulate(c, deficiency), simulate(bg, deficiency))
            flag = "  <-- LOW" if cr < MIN_CONTRAST else ""
            print(f"  {role:<14} on {bgname}  contrast {cr:5.2f}:1{flag}")
            worst_cr = min(worst_cr, cr)
    return worst, worst_cr


if __name__ == "__main__":
    failures = []
    for name, accents in ACCENTS.items():
        (pair, worst), worst_cr = verify(name, accents)
        if worst < THRESHOLD:
            failures.append((name, f"{pair[0]} vs {pair[1]}", f"dE {worst:.1f}"))
        if worst_cr < MIN_CONTRAST:
            failures.append((name, "canvas contrast", f"{worst_cr:.2f}:1"))
        doc = build(accents)
        doc["_comment"] = (
            f"{name}: colours verified distinguishable under Brettel 1997 "
            f"simulation of {DEFICIENCY_OF[name]}. Worst must-differ pair "
            f"{pair[0]}/{pair[1]} at dE {worst:.1f}; worst playback contrast "
            f"against the canvas {worst_cr:.2f}:1. Generated by "
            f"generate-cvd-skins.py; base hues from the Okabe-Ito palette."
        )
        path = os.path.join(HERE, f"{name}Skin.json")
        with open(path, "w") as f:
            json.dump(doc, f, indent=1)
            f.write("\n")

    print()
    if failures:
        for name, what, value in failures:
            print(f"FAIL {name}: {what} = {value}")
        sys.exit(1)
    print(
        f"{len(ACCENTS)} skins written; every must-differ pair clears dE "
        f"{THRESHOLD} and every playback colour clears {MIN_CONTRAST}:1 "
        f"against the canvas, under simulation.")
