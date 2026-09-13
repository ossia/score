#!/usr/bin/env python3
"""Perceptual contrast model for score's skins.

Two metrics, because the skins ask two different questions of their colours.

APCA (Accessible Perceptual Contrast Algorithm, 0.1.9 G-series constants, the
WCAG 3 candidate) answers "can this be read or seen against that". WCAG 2's
contrast ratio is not usable here: it is a fixed ratio of relative luminances,
which overestimates contrast at the dark end by a wide margin, and score's UI
is dark throughout. APCA models the polarity and the spatial response instead,
so light-on-dark and dark-on-light are not the same number. Lc is signed --
negative is light text on a dark ground -- and only the magnitude is compared.

CAM16-UCS answers "are these two colours told apart", which is what the port,
cable and state colours need: they are the same size and lightness and carry
meaning by hue alone. CIELAB dE76 is not adequate for that (its hue spacing is
badly non-uniform in blue), and CIEDE2000 is a small-difference formula being
used far outside its fitted range. CAM16-UCS is uniform over the whole gamut.

Import this from the generators, or run it for the report:
  python3 contrast.py             report every skin against DefaultSkin
  python3 contrast.py --failures  only the pairs that regress or fail
  python3 contrast.py --fix Foo   rewrite a hand-written skin through the
                                  same repair the generators apply
"""

import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


# ----------------------------------------------------------------- colour ops
def parse(c):
    """A skin's [r,g,b] or [r,g,b,a] to a 4-tuple."""
    return (c[0], c[1], c[2], c[3] if len(c) > 3 else 255)


def over(fg, bg):
    """fg composited onto opaque bg."""
    a = fg[3] / 255.0
    return tuple(round(fg[i] * a + bg[i] * (1 - a)) for i in range(3)) + (255,)


def qt_darker(c, factor=200):
    """QColor::darker: HSV value scaled by 100/factor."""
    h, s, v = rgb_to_hsv(c)
    return hsv_to_rgb(h, s, v * 100.0 / factor) + (c[3],)


def qt_lighter(c, factor=150):
    """QColor::lighter: HSV value scaled by factor/100, saturation backing off
    once the value clips, which is how Qt keeps lightening a saturated colour."""
    h, s, v = rgb_to_hsv(c)
    v = v * factor / 100.0
    if v > 1.0:
        s = max(0.0, s - (v - 1.0))
        v = 1.0
    return hsv_to_rgb(h, s, v) + (c[3],)


def rgb_to_hsv(c):
    r, g, b = c[0] / 255.0, c[1] / 255.0, c[2] / 255.0
    mx, mn = max(r, g, b), min(r, g, b)
    d = mx - mn
    if d == 0:
        h = 0.0
    elif mx == r:
        h = ((g - b) / d) % 6
    elif mx == g:
        h = (b - r) / d + 2
    else:
        h = (r - g) / d + 4
    return h * 60.0, (0.0 if mx == 0 else d / mx), mx


def hsv_to_rgb(h, s, v):
    c = v * s
    x = c * (1 - abs((h / 60.0) % 2 - 1))
    m = v - c
    i = int(h // 60) % 6
    r, g, b = [(c, x, 0), (x, c, 0), (0, c, x), (0, x, c), (x, 0, c), (c, 0, x)][i]
    return tuple(max(0, min(255, round((z + m) * 255))) for z in (r, g, b))


# ------------------------------------------------------------------ APCA 0.1.9
# Myndex APCA-W3 0.1.9, the constants the WCAG 3 draft carries. Do not tune
# these individually: they are fitted together.
_MAIN_TRC = 2.4
_CO = (0.2126729, 0.7151522, 0.0721750)
_NORM_BG, _NORM_TXT = 0.56, 0.57
_REV_TXT, _REV_BG = 0.62, 0.65
_BLK_THRS, _BLK_CLMP = 0.022, 1.414
_SCALE_BOW, _SCALE_WOB = 1.14, 1.14
_LO_BOW_OFFSET, _LO_WOB_OFFSET = 0.027, 0.027
_DELTA_Y_MIN, _LO_CLIP = 0.0005, 0.1


def _apca_y(c):
    y = sum(_CO[i] * (c[i] / 255.0) ** _MAIN_TRC for i in range(3))
    return y if y >= _BLK_THRS else y + (_BLK_THRS - y) ** _BLK_CLMP


def apca(fg, bg):
    """Lightness contrast Lc of fg on bg, signed; negative is light-on-dark."""
    ytxt, ybg = _apca_y(fg), _apca_y(bg)
    if abs(ybg - ytxt) < _DELTA_Y_MIN:
        return 0.0
    if ybg > ytxt:  # dark text on a light ground
        s = (ybg**_NORM_BG - ytxt**_NORM_TXT) * _SCALE_BOW
        return 0.0 if s < _LO_CLIP else (s - _LO_BOW_OFFSET) * 100.0
    s = (ybg**_REV_BG - ytxt**_REV_TXT) * _SCALE_WOB
    return 0.0 if s > -_LO_CLIP else (s + _LO_WOB_OFFSET) * 100.0


# ------------------------------------------------------------------- CAM16-UCS
# Viewing conditions: a dark UI on an average-surround display. La is the
# adapting luminance, 20% of a 160 cd/m^2 display white.
_XYZ_W = (95.047, 100.0, 108.883)
_LA = 160.0 * 0.2 / math.pi
_YB = 20.0
_SURROUND = (0.69, 0.59, 1.0)  # average: F, c, Nc

_M16 = (
    (0.401288, 0.650173, -0.051461),
    (-0.250268, 1.204414, 0.045854),
    (-0.002079, 0.048952, 0.953127),
)


def _srgb_to_xyz(c):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4

    r, g, b = lin(c[0]), lin(c[1]), lin(c[2])
    return (
        100 * (0.4124564 * r + 0.3575761 * g + 0.1804375 * b),
        100 * (0.2126729 * r + 0.7151522 * g + 0.0721750 * b),
        100 * (0.0193339 * r + 0.1191920 * g + 0.9503041 * b),
    )


def _cam16_constants():
    f, c, nc = _SURROUND
    k = 1.0 / (5.0 * _LA + 1.0)
    fl = 0.2 * k**4 * (5.0 * _LA) + 0.1 * (1.0 - k**4) ** 2 * (5.0 * _LA) ** (1 / 3)
    n = _YB / _XYZ_W[1]
    z = 1.48 + math.sqrt(n)
    nbb = ncb = 0.725 * (1.0 / n) ** 0.2
    d = min(max(f * (1.0 - (1.0 / 3.6) * math.exp((-_LA - 42.0) / 92.0)), 0.0), 1.0)
    rw, gw, bw = (sum(_M16[i][j] * _XYZ_W[j] for j in range(3)) for i in range(3))
    dr = d * _XYZ_W[1] / rw + 1.0 - d
    dg = d * _XYZ_W[1] / gw + 1.0 - d
    db = d * _XYZ_W[1] / bw + 1.0 - d

    def adapt(v):
        x = (fl * abs(v) / 100.0) ** 0.42
        return math.copysign(400.0 * x / (x + 27.13), v) + 0.1

    aw = (2.0 * adapt(rw * dr) + adapt(gw * dg) + adapt(bw * db) / 20.0 - 0.305) * nbb
    return dict(c=c, nc=nc, fl=fl, n=n, z=z, nbb=nbb, ncb=ncb,
                dr=dr, dg=dg, db=db, aw=aw, adapt=adapt)


_CAM = _cam16_constants()


def cam16_ucs(c):
    """(J', a', b') in CAM16-UCS."""
    x, y, z = _srgb_to_xyz(c)
    r, g, b = (sum(_M16[i][j] * (x, y, z)[j] for j in range(3)) for i in range(3))
    ra = _CAM["adapt"](r * _CAM["dr"])
    ga = _CAM["adapt"](g * _CAM["dg"])
    ba = _CAM["adapt"](b * _CAM["db"])

    a = ra - 12.0 * ga / 11.0 + ba / 11.0
    bb = (ra + ga - 2.0 * ba) / 9.0
    h = math.degrees(math.atan2(bb, a)) % 360.0

    e_t = 0.25 * (math.cos(math.radians(h) + 2.0) + 3.8)
    achr = (2.0 * ra + ga + ba / 20.0 - 0.305) * _CAM["nbb"]
    jj = 100.0 * (achr / _CAM["aw"]) ** (_CAM["c"] * _CAM["z"])
    t = (50000.0 / 13.0 * _CAM["nc"] * _CAM["ncb"] * e_t * math.hypot(a, bb)) / (
        ra + ga + 21.0 * ba / 20.0 + 0.305
    )
    alpha = t**0.9 * (1.64 - 0.29**_CAM["n"]) ** 0.73
    cc = alpha * math.sqrt(jj / 100.0)
    m = cc * _CAM["fl"] ** 0.25

    jp = 1.7 * jj / (1.0 + 0.007 * jj)
    mp = math.log(1.0 + 0.0228 * m) / 0.0228
    return jp, mp * math.cos(math.radians(h)), mp * math.sin(math.radians(h))


def delta_e(c1, c2):
    """CAM16-UCS dE'. Roughly: 1 is a just-noticeable difference, 5 is easy."""
    a, b = cam16_ucs(c1), cam16_ucs(c2)
    return math.dist(a, b)


# ------------------------------------------------------------------ the model
# Every pair below is a thing score actually paints on another thing; the role
# names come from Process::Style in ScenarioStyle.hpp. `kind` says what the
# mark is, which sets the floor: APCA asks more of text than of a wide fill.
#
# (label, foreground role, background role, kind)
PAIRS = [
    # The scenario canvas is Background1; these sit directly on it.
    ("interval body",        "Base1",      "Background1", "shape"),
    ("interval selected",    "Base2",      "Background1", "shape"),
    ("interval playing",     "Base3",      "Background1", "shape"),
    ("interval loop mark",   "Warn1",      "Background1", "shape"),
    ("interval invalid",     "Warn3",      "Background1", "shape"),
    ("interval muted",       "HalfDark",   "Background1", "shape"),
    ("interval label",       "Gray",       "Background1", "text"),
    ("event default",        "Emphasis4",  "Background1", "shape"),
    ("event waiting",        "HalfLight",  "Background1", "shape"),
    ("event pending",        "Warn1",      "Background1", "shape"),
    ("event happened",       "Base3",      "Background1", "shape"),
    ("event disposed",       "Warn3",      "Background1", "shape"),
    ("timesync default",     "Gray",       "Background1", "shape"),
    ("state outline",        "Light",      "Background1", "shape"),
    ("state dot",            "Base1",      "Background1", "shape"),
    ("condition default",    "Smooth3",    "Background1", "shape"),
    ("condition false",      "Smooth1",    "Background1", "shape"),
    ("condition true",       "Smooth2",    "Background1", "shape"),
    ("slot header",          "Base5",      "Background1", "shape"),
    ("process view border",  "Gray",       "Background1", "thin"),

    # Playback, which is drawn over the interval body rather than the canvas.
    # "fill", not "shape": this is the one comparison the eye makes at a
    # glance while something runs, and a whole interval's width of it.
    ("play fill on body",    "Base3",      "Base1",       "fill"),
    # The dashes are also seen against the canvas either side of the interval.
    ("play dash on canvas",  "Pulse1",     "Background1", "thin"),
    ("waiting dash canvas",  "Pulse2",     "Background1", "thin"),
    ("play dash on panel",   "Pulse1",     "Background2", "thin"),
    ("waiting dash panel",   "Pulse2",     "Background2", "thin"),
    ("play dash on fill",    "Pulse1",     "Base3",       "thin"),
    ("waiting dash on body", "Pulse2",     "Base1",       "thin"),
    ("header text on body",  "Light",      "Base1",       "text"),
    ("header side border",   "Emphasis1",  "Base1",       "thin"),

    # The measure grid, from Timebar.hpp: LightBars paints DarkGray and
    # LighterBars its darker300 variant. Deliberately faint, so they are held
    # to a colour difference rather than to a lightness floor.
    ("grid bar",             "DarkGray",   "Background1", "hint"),
    ("grid subdivision",     "DarkGray|d300", "Background1", "hint"),

    # The time ruler has its own ground.
    ("ruler marks",          "Base1",      "DarkGray",    "thin"),
    ("local ruler marks",    "Gray",       "DarkGray",    "thin"),

    # Cables are translucent over the canvas; ports are darkened discs.
    ("audio cable",          "Cable1",     "Background1", "thin"),
    ("data cable",           "Cable2",     "Background1", "thin"),
    ("midi cable",           "Cable3",     "Background1", "thin"),
    ("texture cable",        "LightGray",  "Background1", "thin"),
    ("geometry cable",       "Emphasis3",  "Background1", "thin"),
    ("audio port",           "Port1|dark", "Background2", "shape"),
    ("data port",            "Port2|dark", "Background2", "shape"),
    ("midi port",            "Port3|dark", "Background2", "shape"),
    ("audio port ring",      "Port1",      "Background2", "thin"),
    ("data port ring",       "Port2",      "Background2", "thin"),
    ("midi port ring",       "Port3",      "Background2", "thin"),
]

# APCA floors. The published guidance is 60 for body text and 45 for large
# text; 30 is the "spot reading" level the draft gives for non-text elements
# that carry meaning. A one-pixel line needs more than a wide fill of the same
# colour does, hence the split.
# A floor of zero means the mark is meant to be faint and is judged on
# colour difference alone.
FLOOR = {"text": 45.0, "thin": 30.0, "shape": 20.0, "fill": 35.0, "hint": 0.0}

# A mark can also be found by hue alone: APCA measures lightness only, and
# reports 0 for two colours of the same luminance however different they look.
# So a pair fails only when it is neither light enough nor coloured enough --
# except text, which needs the lightness whatever its hue.
HUE_FLOOR = {"text": None, "thin": 25.0, "shape": 15.0, "fill": 45.0,
             "hint": 4.5}

# Colours whose whole job is to be told apart from each other.
# (name, roles, minimum pairwise dE). 12 is comfortably above "a different
# colour" at these patch sizes. The last group asks for 30, around what
# separates blue from green: idle, running and failed is the reading taken at
# a glance across a whole score, and confusing them is expensive. DefaultSkin
# keeps those 43.9 apart; a palette as muted as Everforest cannot reach that
# without being bleached, and lands just over the bar.
GROUPS = [
    ("port types",      ["Port1", "Port2", "Port3"], 12.0),
    ("cable types",     ["Cable1", "Cable2", "Cable3"], 12.0),
    ("condition state", ["Smooth1", "Smooth2", "Smooth3"], 12.0),
    ("interval state",  ["Base1", "Base2", "Base3"], 12.0),
    ("warning level",   ["Warn1", "Warn2", "Warn3"], 12.0),
    ("idle/running/failed", ["Base1", "Base3", "Warn3"], 30.0),
]
MIN_DE = 12.0  # the default, for anything that does not state its own

# The surfaces everything else is painted on. Moving one of these moves every
# mark that sits on it, so the repair pass leaves them exactly as the palette
# author wrote them.
GROUNDS = {"Background1", "Background2"}

# Roles that mean "this is happening": the play fill, its dash, and the lit
# state of a toggle. Contrast can be had by going either way, and the repair
# takes the nearest -- which on a light interval body means darkening the
# thing that is running. Brighter reads as more active, so these prefer it
# and only darken when nothing brighter will do.
PREFER_BRIGHT = {"Base3", "Pulse1", "Base4"}

# Roles whose hue is the meaning: the severity of a warning, the type of a
# port or cable, the state of a condition or an interval. Draining one of
# these is how a red error becomes a white one, so the repair may not. The
# rest -- the playback dashes, the greys, the surfaces -- carry their meaning
# by position or animation and can be lightened as far as it takes.
SEMANTIC_HUE = {
    "Warn1", "Warn2", "Warn3",
    "Smooth1", "Smooth2", "Smooth3",
    "Tender1", "Tender2", "Tender3",
    "Port1", "Port2", "Port3",
    "Cable1", "Cable2", "Cable3",
    "SelectedCable1", "SelectedCable2", "SelectedCable3",
    "Base1", "Base2", "Base3",
    "Emphasis3",
}

# Pairs a palette cannot satisfy without giving up something worth more than
# the contrast. Listed rather than quietly tolerated, so that a new failure is
# still a failure.
ACCEPTED = {
    # Blue ports on a blue panel. Lightening the disc far enough means
    # draining the blue that says "midi", and the ring carries the
    # identification anyway.
    ("SolarizedDark", "midi port"),
    ("Nord", "midi port"),
    # Magenta play fill on a teal body: both are fixed by the palette's own
    # colour-blind contract, which outranks the lightness here.
    ("ColorBlind", "play fill on body"),
    # Everforest's red and purple are 29 degrees apart in the source palette
    # and both dusty; at the cables' 53% alpha there is nothing left to
    # separate without inventing a hue the palette does not have.
    ("EverforestDark", "cable types"),
}


def split_spec(spec):
    """"Role|modifier" to (role, modifier)."""
    role, _, mod = spec.partition("|")
    return role, mod


def apply_mod(c, mod):
    if mod == "dark":
        return qt_darker(c)
    if mod == "d300":
        return qt_darker(c, 150)
    return c


def resolve(skin, spec, bg):
    """A PAIRS entry's role name to a composited opaque colour."""
    role, mod = split_spec(spec)
    c = apply_mod(parse(skin[role]), mod)
    return over(c, bg) if c[3] < 255 else c


def measure(skin):
    """{label: (Lc, dE)} per pair, plus {~group: min dE} per group."""
    out = {}
    for label, fg, bg, _kind in PAIRS:
        bgc = parse(skin[bg])
        fgc = resolve(skin, fg, bgc)
        out[label] = (apca(fgc, bgc), delta_e(fgc, bgc))
    for name, roles, _min in GROUPS:
        cs = [parse(skin[r]) for r in roles]
        bgc = parse(skin["Background1"])
        cs = [over(c, bgc) if c[3] < 255 else c for c in cs]
        out["~" + name] = min(
            delta_e(cs[i], cs[j])
            for i in range(len(cs))
            for j in range(i + 1, len(cs))
        )
    return out


# ------------------------------------------------------------------- repairing
def _blend(c, target, t):
    return tuple(round(c[i] + (target[i] - c[i]) * t) for i in range(3)) + (c[3],)


def _keeps_chroma(cand, orig, guarded=True):
    """A repair may lighten a colour, not drain it.

    Blending toward white is the only way to lift some dark colours far
    enough, but taken too far it turns a red warning into a white one -- the
    mark loses the meaning it was carrying. Half the original saturation is
    the most this will spend.
    """
    if not guarded:
        return True
    _h, s0, _v = rgb_to_hsv(orig)
    if s0 < 0.25:
        return True
    _h, s1, _v = rgb_to_hsv(cand)
    return s1 >= s0 * 0.5


def _candidates(c):
    """Lighter and darker versions of c, nearest first.

    Scaling the HSV value keeps the hue and the saturation, so it is tried
    first. A dark colour cannot reach white that way -- the value clips long
    before the saturation is gone -- so blends toward white and black follow,
    which a mark on several grounds at once sometimes needs.
    """
    out = []
    for i in range(1, 81):
        out.append(_scale_value(c, 1.0 + 0.03 * i))
        out.append(_scale_value(c, 1.0 / (1.0 + 0.02 * i)))
    for i in range(1, 21):
        out.append(_blend(c, (255, 255, 255), i / 20.0))
        out.append(_blend(c, (0, 0, 0), i / 20.0))
    return out


def _scale_value(c, k):
    """Scale HSV value by k, keeping hue. Past white, desaturate instead, which
    is the only way left to keep lightening a colour that has clipped."""
    h, sat, v = rgb_to_hsv(c)
    v *= k
    if v > 1.0:
        sat = max(0.0, sat - (v - 1.0))
        v = 1.0
    return hsv_to_rgb(h, sat, v) + (c[3],)


# How much lightness contrast the repair will chase. It stops at the cap even
# when DefaultSkin has more: pushing a muted palette to match a neon one would
# just replace the palette.
CAP = {"text": 60.0, "thin": 45.0, "shape": 45.0, "fill": 45.0, "hint": 0.0}


def target_lc(kind, ref_lc, tol):
    """The Lc to aim for: the floor, or DefaultSkin's less the tolerance."""
    return min(max(FLOOR[kind], abs(ref_lc) - tol), CAP[kind])


def repair_pair(fg, bg, kind, opaque_bg=None, want=None):
    """fg moved the least distance that clears the floors against bg.

    Lightness only: the hue is the palette's identity and is not ours to
    change. Both directions are tried and the nearer result wins, so a colour
    on a mid ground goes whichever way it was already leaning.
    """
    comp = (lambda c: over(c, opaque_bg)) if opaque_bg else (lambda c: c)
    want = FLOOR[kind] if want is None else want

    def ok(c):
        cc = comp(c)
        lc, de = apca(cc, bg), delta_e(cc, bg)
        if abs(lc) >= want:
            return True
        # Hue can stand in for lightness, but only down to the hard floor.
        hue = HUE_FLOOR[kind]
        return hue is not None and de >= hue and abs(lc) >= FLOOR[kind]

    if ok(fg):
        return fg

    best = None
    # 2% steps, out to four times and down to a sixteenth of the value.
    for i in range(1, 81):
        for k in (1.0 + 0.02 * i * 1.5, 1.0 / (1.0 + 0.02 * i)):
            if not (1 / 16.0 <= k <= 4.0):
                continue
            cand = _scale_value(fg, k)
            if ok(cand):
                d = delta_e(cand, fg)
                if best is None or d < best[0]:
                    best = (d, cand)
        if best is not None:
            return best[1]
    return fg


def fails_values(kind, lc, de):
    if abs(lc) >= FLOOR[kind]:
        return False
    hue = HUE_FLOOR[kind]
    return hue is None or de < hue


def _constraints(skin, role, rm, tol, own=None):
    """Every (bg, kind, want_lc, want_de, modifier) this role must satisfy."""
    out = []
    for label, fgspec, bgrole, kind in PAIRS:
        r, mod = split_spec(fgspec)
        if r != role:
            continue
        want = target_lc(kind, rm[label][0], tol) if rm else FLOOR[kind]
        de = HUE_FLOOR[kind] or 0.0
        if FLOOR[kind] <= 0 and own:
            # A faint mark is judged on colour difference, so the repair has
            # to keep whatever difference it already had -- fixing one of
            # these would otherwise be free to spend the other.
            ceiling = rm[label][1] if rm else de
            de = max(de, min(own[label][1], ceiling))
        out.append((parse(skin[bgrole]), kind, want, de, mod))
    return out


def _satisfies(colour, cons):
    for bg, kind, want, want_de, mod in cons:
        c = apply_mod(colour, mod)
        if c[3] < 255:
            c = over(c, bg)
        lc, de = apca(c, bg), delta_e(c, bg)
        if want > 0 and abs(lc) >= want:
            continue
        if HUE_FLOOR[kind] is not None and de >= want_de and abs(lc) >= FLOOR[kind]:
            continue
        return False
    return True


def repair(skin, ref=None, tol=12.0, origin=None):
    """Lift every role that cannot be seen where score paints it.

    Solved per role rather than per pair: a role is usually painted on more
    than one ground -- the waiting dash crosses both the interval and the
    canvas either side of it -- and satisfying those one at a time just moves
    the colour back and forth between them. One scale factor has to clear all
    of them at once, so they are searched together and the nearest wins.

    The grounds themselves are never touched: moving the canvas would move
    every mark that sits on it.
    """
    skin = dict(skin)
    origin = origin or skin
    rm = measure(ref) if ref else None
    own = measure(skin)
    roles = []
    for _label, fgspec, _bg, _kind in PAIRS:
        r, _mod = split_spec(fgspec)
        if r not in GROUNDS and r not in roles:
            roles.append(r)

    for role in roles:
        cons = _constraints(skin, role, rm, tol, own)
        cur = parse(skin[role])
        if _satisfies(cur, cons):
            continue
        best = None
        j0 = cam16_ucs(cur)[0]
        bright = role in PREFER_BRIGHT
        guarded = role in SEMANTIC_HUE
        src = parse(origin.get(role, skin[role]))
        for cand in _candidates(cur):
            # Against the palette's own colour, not the current one: a guard
            # that only looks one step back lets the saturation compound away
            # over the rounds.
            if _keeps_chroma(cand, src, guarded) and _satisfies(cand, cons):
                # Sorts darker candidates behind brighter ones for the roles
                # that should read as active, and by distance otherwise.
                key = (0 if (not bright or cam16_ucs(cand)[0] >= j0) else 1,
                       delta_e(cand, cur))
                if best is None or key < best[0]:
                    best = (key, cand)
        if best is not None:
            skin[role] = list(best[1][:3]) + (
                [cur[3]] if len(skin[role]) > 3 else [])
    return skin


def separate(skin, passes=80, origin=None):
    """Push apart roles whose whole job is to be told from each other.

    Lightness again, and both members of the closest pair move, so a group of
    three ends up as a ramp rather than one outlier. Hue is left alone: these
    are the palette's accents, and a protanope cannot use hue anyway -- which
    is the reason a group that relies on it alone counts as a failure here.
    """
    skin = dict(skin)
    origin = origin or skin
    for _name, roles, want in GROUPS:
        bgc = parse(skin["Background1"])
        for _ in range(passes):
            cs = []
            for r in roles:
                c = parse(skin[r])
                cs.append(over(c, bgc) if c[3] < 255 else c)
            worst = min(
                ((delta_e(cs[i], cs[j]), i, j)
                 for i in range(len(cs)) for j in range(i + 1, len(cs))),
                key=lambda t: t[0],
            )
            if worst[0] >= want:
                break
            _d, i, j = worst
            hi, lo = (i, j) if cam16_ucs(cs[i])[0] >= cam16_ucs(cs[j])[0] else (j, i)
            for idx, k in ((hi, 1.06), (lo, 1 / 1.06)):
                cur = parse(skin[roles[idx]])
                nxt = _scale_value(cur, k)
                # Same rule as the repair: separating two colours must not
                # bleach either of them into a neutral.
                src = parse(origin.get(roles[idx], skin[roles[idx]]))
                if not _keeps_chroma(nxt, src, roles[idx] in SEMANTIC_HUE):
                    continue
                skin[roles[idx]] = list(nxt[:3]) + (
                    [cur[3]] if len(skin[roles[idx]]) > 3 else [])
    return skin


def improve(skin, ref=None, rounds=6):
    """Lift and separate until it stops getting better.

    The two passes pull against each other -- separating a group moves its
    members' contrast, and lifting two members towards the same target brings
    them back together -- so they alternate, and the best round wins rather
    than the last.
    """
    ref = ref or skin
    origin = dict(skin)
    best, best_n = skin, len(audit(skin, ref))
    cur = skin
    for _ in range(rounds):
        cur = repair(separate(cur, origin=origin), ref, origin=origin)
        n = len(audit(cur, ref))
        if n < best_n:
            best, best_n = cur, n
        if n == 0:
            break
    return best


def load(stem):
    with open(os.path.join(HERE, f"{stem}.json")) as f:
        return json.load(f)


def kind_of(label):
    for lbl, _f, _b, k in PAIRS:
        if lbl == label:
            return k
    return None


def fails(label, lc, de):
    """Neither light enough nor coloured enough to be found."""
    kind = kind_of(label)
    if FLOOR[kind] > 0 and abs(lc) >= FLOOR[kind]:
        return False
    hue = HUE_FLOOR[kind]
    return hue is None or de < hue


def audit(skin, ref, tol=12.0, name=None):
    """Problems in `skin` against `ref`: (label, lc, de, ref_lc, ref_de, why).

    A pair the reference also fails is not this skin's doing, so it is only
    reported when this skin makes it worse.
    """
    m, r = measure(skin), measure(ref)
    bad = []
    for label, v in m.items():
        if name and (name, label.lstrip("~")) in ACCEPTED:
            continue
        if label.startswith("~"):
            want = dict((n, m) for n, _r, m in GROUPS)[label[1:]]
            if v < want and r[label] >= want:
                bad.append((label, v, 0.0, r[label], 0.0, "indistinct"))
            continue
        lc, de = v
        rlc, rde = r[label]
        if fails(label, lc, de) and not fails(label, rlc, rde):
            bad.append((label, lc, de, rlc, rde, "below floor"))
        elif abs(rlc) - abs(lc) > tol and abs(lc) < FLOOR[kind_of(label)]:
            bad.append((label, lc, de, rlc, rde, "regressed"))
    return bad


def fix(stems):
    """Rewrite skins through improve(). For the hand-written ones -- the
    generated skins get the same treatment as they are written."""
    ref = load("DefaultSkin")
    for stem in stems:
        path = os.path.join(HERE, f"{stem}.json")
        with open(path) as f:
            doc = json.load(f)
        name = stem[:-4]
        before = len(audit(doc, ref, name=name))
        out = improve(doc, ref)
        with open(path, "w") as f:
            json.dump(out, f, indent=1)
            f.write("\n")
        print(f"{stem}: {before} -> {len(audit(out, ref, name=name))} problem(s)")


def main():
    if "--fix" in sys.argv:
        args = [a for a in sys.argv[1:] if not a.startswith("-")]
        return fix(args or [
            f[:-5] for f in sorted(os.listdir(HERE))
            if f.endswith("Skin.json") and f != "DefaultSkin.json"
        ])

    only_bad = "--failures" in sys.argv
    ref = load("DefaultSkin")
    stems = sorted(
        f[:-5] for f in os.listdir(HERE)
        if f.endswith("Skin.json") and f != "DefaultSkin.json"
    )

    print("DefaultSkin, for reference. Lc is APCA lightness contrast, negative")
    print("for light on dark; dE is CAM16-UCS colour difference.\n")
    m = measure(ref)
    for label, _fg, _bg, kind in PAIRS:
        lc, de = m[label]
        flag = "  <-- neither" if fails(label, lc, de) else ""
        print(f"  {label:<22} Lc {lc:7.1f} / {FLOOR[kind]:<4.0f}"
              f"  dE {de:6.1f}{flag}")
    for name, _roles, want in GROUPS:
        v = m["~" + name]
        flag = "  <-- indistinct" if v < want else ""
        print(f"  {name:<22} dE {v:6.1f}   min {want:.0f}{flag}")

    print("\n\nEvery other skin, against that\n")
    total = 0
    for stem in stems:
        bad = audit(load(stem), ref, name=stem[:-4])
        total += len(bad)
        if not bad and only_bad:
            continue
        print(f"{stem}: {len(bad)} problem(s)")
        for label, lc, de, rlc, rde, why in bad:
            if label.startswith("~"):
                print(f"    {label.lstrip('~'):<22} dE {lc:6.1f}"
                      f"   default {rlc:6.1f}   {why}")
            else:
                print(f"    {label:<22} Lc {lc:7.1f} dE {de:6.1f}"
                      f"   default Lc {rlc:7.1f} dE {rde:6.1f}   {why}")
    print(f"\n{total} problem(s) across {len(stems)} skins")


if __name__ == "__main__":
    main()
