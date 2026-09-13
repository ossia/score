#!/usr/bin/env python3
"""Generate one skin per shipped pixel font, at each usable size.

A pixel font is only sharp at a whole multiple of its design grid, so the
sizes here are not free parameters: they are the grid, and integer multiples
of it. Everything else gives uneven stem widths. The grids were measured from
the font outlines; see /tmp/qtfont/grid.py in the investigation, or just
remember that every coordinate in these fonts is a multiple of upm/grid.

Families that ship several grid sizes (Galmuri, Ark Pixel) get a skin that
uses the siblings for the small and title roles, which is how you get visual
hierarchy without taking one font off its grid. Single-grid families use one
size throughout, with the title at double.

Only 1x: a doubled pixel font is just a chunkier pixel font, and the sizes it
lands on are already covered by the next family up.

Run from this directory:  python3 generate-font-skins.py
Then add any new file to ../score.qrc.
"""

import glob
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# Palette shared by all generated skins: whatever DefaultSkin uses.
with open(os.path.join(HERE, "DefaultSkin.json")) as f:
    DEFAULT = json.load(f)
PALETTE = {k: v for k, v in DEFAULT.items() if not k.startswith("_") and k != "fonts"}

ROLES = [
    "application", "sans", "sansSmall", "mono", "monoSmall",
    "bold10", "bold12", "medium7", "medium8", "medium10", "medium12",
    "title", "sectionTitle", "slider", "ruler", "code", "timecode",
]


# Mirrors pixelFontGrid() in Skin.cpp. A size that is not a whole multiple of
# these puts the outlines between pixels and every stem comes out different.
GRIDS = {
    "Galmuri7": 8, "Galmuri9": 10, "Galmuri11": 12, "Galmuri14": 15,
    "GalmuriMono7": 8, "GalmuriMono9": 10, "GalmuriMono11": 12,
    "Departure Mono": 11, "Cozette": 13, "CozetteVector": 13,
    "Ark Pixel 10px Prop latin": 10, "Ark Pixel 12px Prop latin": 12,
    "Ark Pixel 16px Prop latin": 16,
}


def check(stem, fonts):
    """Refuses to write a skin that would render with uneven stems."""
    for role, spec in fonts.items():
        family, px = spec.get("family"), spec.get("pixelSize")
        grid = GRIDS.get(family)
        if grid and px % grid:
            raise SystemExit(
                f"{stem}: {role} asks for {family} at {px} px, "
                f"which is not a multiple of its {grid} px grid"
            )
        if grid and family not in HAS_BOLD_FACE and spec.get("bold"):
            raise SystemExit(
                f"{stem}: {role} asks for bold {family}, which has no bold "
                f"face, so Qt would smear the glyphs"
            )
        if grid and "bold" not in spec and "weight" not in spec:
            raise SystemExit(
                f"{stem}: {role} pins no weight, so it inherits the one "
                f"Skin::setupFonts() left on that role"
            )


# Families that ship a Bold face; Qt smears the glyphs to fake one on any
# other. Galmuri11's is in the same family, so it needs a style name.
HAS_BOLD_FACE = {"Galmuri11"}


def emphasis(family, px):
    """The heaviest face this family has, without Qt faking one."""
    spec = {"family": family, "pixelSize": px}
    if family in HAS_BOLD_FACE:
        spec["styleName"] = "Bold"
        spec["bold"] = True
    else:
        spec["bold"] = False
    return spec


def plain(family, px, **extra):
    """A role at its plain weight.

    Every role writes "bold": false. load() rebuilds through setupFonts()
    first, which leaves weight 900 on mono and 600 on slider, and a role
    naming only a family and a size inherits those.
    """
    return {"family": family, "pixelSize": px, "bold": False, **extra}


def graded(name, small, body, large, mono, mono_small, scale):
    """A family with several hand-drawn grid sizes."""
    s, b, l = (small[1] * scale, body[1] * scale, large[1] * scale)
    return {
        "application": plain(body[0], b),
        "sans": plain(body[0], b),
        "sansSmall": plain(small[0], s),
        "mono": plain(mono[0], mono[1] * scale),
        "monoSmall": plain(mono_small[0], mono_small[1] * scale),
        "bold10": emphasis(body[0], b),
        "bold12": emphasis(large[0], l),
        "medium7": plain(small[0], s),
        "medium8": plain(small[0], s),
        "medium10": plain(body[0], b),
        "medium12": plain(body[0], b),
        # Hierarchy comes from the larger grid size, not from a faked weight.
        "title": emphasis(large[0], l),
        # One grid step above the body, like the panel banner.
        "sectionTitle": emphasis(large[0], l),
        "slider": plain(small[0], s),
        # Smallest text on screen and in columns: the smallest grid the
        # family has, monospaced where there is one.
        "ruler": plain(mono_small[0], mono_small[1] * scale),
        # Code wants the monospaced face at the body size.
        "code": plain(mono[0], mono[1] * scale, fixedPitch=True),
        # Largest text on screen, so an off-grid size shows.
        "timecode": emphasis(body[0], b),
    }


# A single strike, ignoring setPixelSize: cozette.bdf is 13 px whatever you
# ask for, so these must not claim a larger title.
BITMAP_ONLY = {"Cozette"}


def uniform(family, grid, scale, big_title=True):
    """A family with a single grid: one size everywhere.

    With big_title, the title and bold12 roles go to double the grid, which is
    still exact. Without it every role stays at one size, for when no row of
    pixels can be spared.
    """
    px = grid * scale
    # A bitmap family cannot be scaled, so asking for a bigger title would
    # just produce the same size with a misleading number in the file.
    large = px * 2 if (big_title and family not in BITMAP_ONLY) else px
    fonts = {}
    for role in ROLES:
        fonts[role] = plain(family, px)
    fonts["bold10"] = emphasis(family, px)
    fonts["bold12"] = emphasis(family, large)
    fonts["title"] = emphasis(family, large)
    # Not doubled, unlike the panel banner: this heading sits over every
    # inspector page, and with only one grid to work with the alternative to
    # the body size is twice it, which reads as a title bar rather than a
    # heading. It takes the family's real bold instead, where there is one.
    fonts["sectionTitle"] = emphasis(family, px)
    # One grid, so the ruler gets the same size as everything else.
    fonts["ruler"] = plain(family, px)
    fonts["timecode"] = emphasis(family, px)
    return fonts


# (file stem, human name, builder)
SKINS = []

for scale, suffix in ((1, ""),):
    SKINS.append((
        f"Galmuri{suffix}",
        graded("Galmuri", ("Galmuri9", 10), ("Galmuri11", 12), ("Galmuri14", 15),
               ("GalmuriMono11", 12), ("GalmuriMono9", 10), scale),
    ))
    SKINS.append((
        f"GalmuriTiny{suffix}",
        graded("GalmuriTiny", ("Galmuri7", 8), ("Galmuri9", 10), ("Galmuri11", 12),
               ("GalmuriMono9", 10), ("GalmuriMono7", 8), scale),
    ))
    SKINS.append((
        f"GalmuriMono{suffix}",
        graded("GalmuriMono", ("GalmuriMono7", 8), ("GalmuriMono9", 10),
               ("GalmuriMono11", 12), ("GalmuriMono11", 12), ("GalmuriMono7", 8),
               scale),
    ))
    SKINS.append((
        f"ArkPixel{suffix}",
        graded("ArkPixel",
               ("Ark Pixel 10px Prop latin", 10),
               ("Ark Pixel 12px Prop latin", 12),
               ("Ark Pixel 16px Prop latin", 16),
               ("Ark Pixel 12px Prop latin", 12),
               ("Ark Pixel 10px Prop latin", 10), scale),
    ))
    SKINS.append((f"DepartureMono{suffix}", uniform("Departure Mono", 11, scale)))
    # The bitmap "Cozette" (cozette.bdf, shipped already) rather than the
    # traced CozetteVector: same metrics on paper, but the bitmap is what the
    # font's author rasterised and it reads better on screen.
    SKINS.append((f"Cozette{suffix}", uniform("Cozette", 13, scale)))

# Micro skins for genuinely small screens, 800x600 and below. Galmuri7 on its
# 8 px grid is the smallest thing score ships, and the density difference is
# large. Measured with QFontMetrics on the string
# "Interval 3 - gain -6.0 dB - 48000 Hz":
#
#   font                px  line height  rows in 600px  cols in 800px
#   Galmuri7             8           10             60            193
#   GalmuriMono7         8           10             60            200
#   Galmuri9            10           12             50            157
#   Galmuri11           12           16             37            123
#   Departure Mono      11           14             42            114
#
SKINS.append((
    "GalmuriMicro",
    graded("GalmuriMicro", ("Galmuri7", 8), ("Galmuri7", 8), ("Galmuri9", 10),
           ("GalmuriMono7", 8), ("GalmuriMono7", 8), 1),
))
SKINS.append((
    "GalmuriMonoMicro",
    graded("GalmuriMonoMicro", ("GalmuriMono7", 8), ("GalmuriMono7", 8),
           ("GalmuriMono9", 10), ("GalmuriMono7", 8), ("GalmuriMono7", 8), 1),
))
# The absolute floor: one size everywhere, nothing larger for titles, for when
# every row of pixels counts.
SKINS.append(("GalmuriNano", uniform("Galmuri7", 8, 1, big_title=False)))

# "Small screen" is the named preset for a cramped display; it is the same
# configuration as GalmuriTiny. Generated rather than hand-written so the two
# cannot drift, which is how the hand-written version ended up asking for a
# bold face that Galmuri14 does not have.
SKINS.append((
    "SmallScreen",
    graded("SmallScreen", ("Galmuri7", 8), ("Galmuri9", 10), ("Galmuri11", 12),
           ("GalmuriMono9", 10), ("GalmuriMono7", 8), 1),
))

# Galmuri11 has real Bold and Condensed faces in the one family, so its skin
# selects them by style name rather than letting Qt fake a bold.
for scale, suffix in ((1, ""),):
    f = graded("Galmuri11", ("Galmuri11", 12), ("Galmuri11", 12), ("Galmuri11", 12),
               ("GalmuriMono11", 12), ("GalmuriMono11", 12), scale)
    for role in ("bold10", "bold12", "title", "sectionTitle"):
        f[role]["styleName"] = "Bold"
    f["sansSmall"]["styleName"] = "Condensed"
    SKINS.append((f"Galmuri11Faces{suffix}", f))


def write(stem, fonts):
    check(stem, fonts)
    doc = dict(PALETTE)
    sizes = sorted({spec["pixelSize"] for spec in fonts.values()})
    doc["_comment"] = (
        f"Generated by generate-font-skins.py. Pixel sizes used: "
        f"{', '.join(str(s) for s in sizes)}. These are grid multiples of the "
        f"fonts involved; other sizes render with uneven stems."
    )
    # Antialiasing off for every role: that is what keeps the pixels square.
    doc["fonts"] = {"defaults": {"antialias": False, "hinting": "None"}, **fonts}
    path = os.path.join(HERE, f"{stem}Skin.json")
    with open(path, "w") as fp:
        json.dump(doc, fp, indent=1)
        fp.write("\n")
    return path, sizes


if __name__ == "__main__":
    for stem, fonts in SKINS:
        path, sizes = write(stem, fonts)
        print(f"{os.path.basename(path):<34} sizes {sizes}")
    print(f"\n{len(SKINS)} skins written. Add them to ../score.qrc.")

    # The hand-written and derived skins are subject to the same rule.
    for other in sorted(glob.glob(os.path.join(HERE, "*Skin.json"))):
        stem = os.path.basename(other)[:-len("Skin.json")]
        if stem in {s for s, _ in SKINS}:
            continue
        with open(other) as fp:
            fonts = json.load(fp).get("fonts", {})
        check(stem, {r: s for r, s in fonts.items() if isinstance(s, dict)})
    print("Every skin on file uses its pixel fonts at grid multiples.")
