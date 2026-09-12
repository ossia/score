#!/usr/bin/env python3
"""Generate colour skins from well-known palettes.

Every hex value below was taken verbatim from the palette's own canonical
source (the upstream repo or the author's published palette), not eyeballed
from a screenshot:

  Gruvbox Dark      morhetz/gruvbox, colors/gruvbox.vim
  Solarized Dark    altercation/solarized, vim-colors-solarized
  Nord              nordtheme/nord, src/nord.css
  Dracula           dracula/vim, autoload/dracula.vim
  Catppuccin Mocha  catppuccin/palette, palette.json
  Tokyo Night       folke/tokyonight.nvim, colors/storm.lua + night.lua
  Everforest Dark   neanias/everforest-nvim, colours.lua (dark, medium)
  Rose Pine         rose-pine/neovim, palette.lua (main)
  Kanagawa Wave     rebelot/kanagawa.nvim, colors.lua
  Evergarden Winter comfysage/evergarden, palettes/winter.lua

Each palette supplies a background ramp, a foreground ramp and eight accents;
`build()` maps those onto score's 42 colour roles. The mapping is in one place
so the schemes stay consistent with each other and with DefaultSkin's
semantics: Dark..Light is a dark-to-light ramp, Base1..4 and Emphasis1/3 are
accents, Warn1..3 go yellow/amber/red, Port1..3 and Cable1..3 identify port
and cable types, and the two Cable sets differ only in alpha.

Run from this directory:  python3 generate-color-skins.py
Then add any new file to ../score.qrc.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# Alpha values DefaultSkin uses for cables; kept so cables stay translucent.
CABLE_ALPHA = 136
SELECTED_CABLE_ALPHA = 204

# Playback legibility thresholds. dE for "these two are different colours",
# WCAG contrast ratio for "this mark is visible on the canvas".
MIN_DE = 10.0
MIN_CONTRAST = 3.0


def rgb(h):
    h = h.lstrip("#")
    return [int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16)]


def relative_luminance(c):
    r, g, b = (srgb_to_linear(x) for x in c[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def srgb_to_linear(v):
    v = v / 255.0
    return v / 12.92 if v < 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def contrast_ratio(a, b):
    la, lb = relative_luminance(a), relative_luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def to_lab(c):
    r, g, b = (srgb_to_linear(x) for x in c[:3])
    x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return [116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)]


def delta_e(a, b):
    return sum((p - q) ** 2 for p, q in zip(to_lab(a), to_lab(b))) ** 0.5


def mix(a, b, t):
    """Blend two hex colours, t=0 gives a, t=1 gives b."""
    ca, cb = rgb(a), rgb(b)
    return [round(x + (y - x) * t) for x, y in zip(ca, cb)]


def lift_to_contrast(colour, toward, backgrounds, target):
    """Blend `colour` toward `toward` until it clears `target` on every bg.

    These palettes' grey is a comment colour, deliberately low contrast, so
    used as-is for the waiting playback dash it disappears against the canvas.
    Rather than hand-pick a replacement per palette, lift it by the smallest
    amount that stays visible; a palette that is already fine is untouched.
    """
    for i in range(0, 21):
        t = i / 20
        c = mix(colour, toward, t)
        if all(contrast_ratio(c, rgb(bg)) >= target for bg in backgrounds):
            return c
    return mix(colour, toward, 1.0)


def build(p):
    """Map a palette onto score's 42 colour roles.

    p needs: bg_dim, bg0, bg1, bg2, bg3, fg, fg_dim, grey,
             red, orange, yellow, green, aqua, blue, purple, pink
    """
    return {
        # Dark to light ramp. Widgets use these for outlines and text, so the
        # ordering matters more than the exact values.
        "Dark": rgb(p["bg_dim"]),
        "HalfDark": rgb(p["bg1"]),
        "DarkGray": rgb(p["bg2"]),
        "Gray": rgb(p["grey"]),
        "LightGray": rgb(p["fg_dim"]),
        "HalfLight": mix(p["fg_dim"], p["fg"], 0.6),
        "Light": rgb(p["fg"]),

        # Emphasis1 is the selection/highlight colour, 3 a secondary accent,
        # 2/5 are surfaces and 4 is the brightest foreground.
        "Emphasis1": rgb(p["aqua"]),
        "Emphasis2": rgb(p["bg2"]),
        "Emphasis3": rgb(p["purple"]),
        "Emphasis4": rgb(p["fg"]),
        "Emphasis5": rgb(p["bg1"]),

        "Base1": rgb(p["aqua"]),
        "Base2": rgb(p["blue"]),
        "Base3": rgb(p["green"]),
        "Base4": rgb(p["yellow"]),
        "Base5": rgb(p["bg0"]),

        "Warn1": rgb(p["yellow"]),
        "Warn2": rgb(p["orange"]),
        "Warn3": rgb(p["red"]),

        "Background1": rgb(p["bg0"]),
        "Background2": rgb(p["bg1"]),

        "Transparent1": rgb(p["bg_dim"]),
        "Transparent2": rgb(p["grey"]),
        "Transparent3": rgb(p["bg2"]),

        "Smooth1": rgb(p["red"]),
        "Smooth2": rgb(p["green"]),
        "Smooth3": rgb(p["yellow"]),

        "Tender1": rgb(p["red"]),
        "Tender2": rgb(p["yellow"]),
        "Tender3": rgb(p["bg3"]),

        # Cables: audio, midi, value. Same hues as the ports, translucent.
        "Cable1": rgb(p["red"]) + [CABLE_ALPHA],
        "Cable2": rgb(p["green"]) + [CABLE_ALPHA],
        "Cable3": rgb(p["purple"]) + [CABLE_ALPHA],
        "SelectedCable1": rgb(p["red"]) + [SELECTED_CABLE_ALPHA],
        "SelectedCable2": rgb(p["green"]) + [SELECTED_CABLE_ALPHA],
        "SelectedCable3": rgb(p["purple"]) + [SELECTED_CABLE_ALPHA],

        "Port1": rgb(p["red"]),
        "Port2": rgb(p["green"]),
        "Port3": rgb(p["blue"]),

        # The playback dash animates on top of Base3, so it must not be
        # Base3. Near-foreground for playing, grey for waiting: both keep
        # their contrast whatever hues the palette happens to use.
        "Pulse1": mix(p["fg"], p["orange"], 0.25),
        "Pulse2": lift_to_contrast(
            p["grey"], p["fg"], (p["bg0"], p["bg1"]), MIN_CONTRAST),
    }


PALETTES = {
    # morhetz/gruvbox colors/gruvbox.vim, dark medium + bright accents
    "GruvboxDark": dict(
        bg_dim="#1d2021", bg0="#282828", bg1="#3c3836", bg2="#504945", bg3="#665c54",
        grey="#928374", fg_dim="#bdae93", fg="#ebdbb2",
        red="#fb4934", orange="#fe8019", yellow="#fabd2f", green="#b8bb26",
        aqua="#8ec07c", blue="#83a598", purple="#d3869b", pink="#d3869b"),

    # altercation/solarized, base03..base3 plus the eight accents
    "SolarizedDark": dict(
        bg_dim="#00212b", bg0="#002b36", bg1="#073642", bg2="#0b4553", bg3="#586e75",
        grey="#657b83", fg_dim="#839496", fg="#93a1a1",
        red="#dc322f", orange="#cb4b16", yellow="#b58900", green="#859900",
        aqua="#2aa198", blue="#268bd2", purple="#6c71c4", pink="#d33682"),

    # nordtheme/nord src/nord.css, nord0..nord15
    "Nord": dict(
        bg_dim="#242933", bg0="#2e3440", bg1="#3b4252", bg2="#434c5e", bg3="#4c566a",
        grey="#616e88", fg_dim="#d8dee9", fg="#eceff4",
        red="#bf616a", orange="#d08770", yellow="#ebcb8b", green="#a3be8c",
        aqua="#8fbcbb", blue="#81a1c1", purple="#b48ead", pink="#b48ead"),

    # dracula/vim autoload/dracula.vim
    "Dracula": dict(
        bg_dim="#191a21", bg0="#282a36", bg1="#343746", bg2="#424450", bg3="#44475a",
        grey="#6272a4", fg_dim="#bcc2cd", fg="#f8f8f2",
        red="#ff5555", orange="#ffb86c", yellow="#f1fa8c", green="#50fa7b",
        aqua="#8be9fd", blue="#8be9fd", purple="#bd93f9", pink="#ff79c6"),

    # catppuccin/palette palette.json, mocha
    "CatppuccinMocha": dict(
        bg_dim="#11111b", bg0="#1e1e2e", bg1="#313244", bg2="#45475a", bg3="#585b70",
        grey="#6c7086", fg_dim="#a6adc8", fg="#cdd6f4",
        red="#f38ba8", orange="#fab387", yellow="#f9e2af", green="#a6e3a1",
        aqua="#94e2d5", blue="#89b4fa", purple="#cba6f7", pink="#f5c2e7"),

    # folke/tokyonight.nvim, night bg over the storm accents
    "TokyoNight": dict(
        bg_dim="#16161e", bg0="#1a1b26", bg1="#24283b", bg2="#292e42", bg3="#3b4261",
        grey="#565f89", fg_dim="#a9b1d6", fg="#c0caf5",
        red="#f7768e", orange="#ff9e64", yellow="#e0af68", green="#9ece6a",
        aqua="#7dcfff", blue="#7aa2f7", purple="#bb9af7", pink="#bb9af7"),

    # neanias/everforest-nvim colours.lua, dark medium
    "EverforestDark": dict(
        bg_dim="#232a2e", bg0="#2d353b", bg1="#343f44", bg2="#3d484d", bg3="#475258",
        grey="#7a8478", fg_dim="#9da9a0", fg="#d3c6aa",
        red="#e67e80", orange="#e69875", yellow="#dbbc7f", green="#a7c080",
        aqua="#83c092", blue="#7fbbb3", purple="#d699b6", pink="#d699b6"),

    # rose-pine/neovim palette.lua, main
    "RosePine": dict(
        bg_dim="#16141f", bg0="#191724", bg1="#1f1d2e", bg2="#26233a", bg3="#403d52",
        grey="#6e6a86", fg_dim="#908caa", fg="#e0def4",
        red="#eb6f92", orange="#ebbcba", yellow="#f6c177", green="#95b1ac",
        aqua="#9ccfd8", blue="#31748f", purple="#c4a7e7", pink="#ebbcba"),

    # rebelot/kanagawa.nvim colors.lua, wave
    "KanagawaWave": dict(
        bg_dim="#16161d", bg0="#1f1f28", bg1="#2a2a37", bg2="#363646", bg3="#54546d",
        grey="#727169", fg_dim="#c8c093", fg="#dcd7ba",
        red="#e46876", orange="#ffa066", yellow="#e6c384", green="#98bb6c",
        aqua="#7aa89f", blue="#7e9cd8", purple="#957fb8", pink="#d27e99"),

    # comfysage/evergarden palettes/winter.lua
    "EvergardenWinter": dict(
        bg_dim="#171c1f", bg0="#1e2528", bg1="#262f33", bg2="#374145", bg3="#4a585c",
        grey="#6f8788", fg_dim="#adc9bc", fg="#f8f9e8",
        red="#f57f82", orange="#f7a182", yellow="#f5d098", green="#cbe3b3",
        aqua="#b3e3ca", blue="#b2caed", purple="#d2bdf3", pink="#f3c0e5"),
}


def write(name, palette):
    doc = build(palette)
    doc["_comment"] = (
        f"{name}: generated by generate-color-skins.py from the palette's "
        f"canonical source. Colours only; the fonts come from whichever font "
        f"skin is active, or the built-in defaults."
    )
    path = os.path.join(HERE, f"{name}Skin.json")
    with open(path, "w") as f:
        json.dump(doc, f, indent=1)
        f.write("\n")
    return path


# What playback looks like: Base3 (the fill that grows across a running
# interval) over Base1 (the interval body), with Pulse1 dashing on top and
# Pulse2 while waiting. All of it has to stay visible.
PLAYBACK_PAIRS = [("Base3", "Base1"), ("Pulse1", "Base3"), ("Pulse1", "Pulse2"),
                  ("Pulse1", "Base1"), ("Base3", "Base2")]
PLAYBACK_ON_CANVAS = ["Base1", "Base3", "Pulse1", "Pulse2"]


def check_playback(name, doc):
    """Report the playback separations; returns a list of problems."""
    bad = []
    for x, y in PLAYBACK_PAIRS:
        d = delta_e(doc[x], doc[y])
        if d < MIN_DE:
            bad.append(f"{x} vs {y} dE {d:.1f}")
    for role in PLAYBACK_ON_CANVAS:
        for bg in ("Background1", "Background2"):
            cr = contrast_ratio(doc[role], doc[bg])
            if cr < MIN_CONTRAST:
                bad.append(f"{role} on {bg} {cr:.2f}:1")
    return bad


if __name__ == "__main__":
    problems = {}
    for name, palette in PALETTES.items():
        doc = build(palette)
        bad = check_playback(name, doc)
        path = write(name, palette)
        worst_de = min(delta_e(doc[x], doc[y]) for x, y in PLAYBACK_PAIRS)
        worst_cr = min(
            contrast_ratio(doc[r], doc[b]) for r in PLAYBACK_ON_CANVAS
            for b in ("Background1", "Background2"))
        print(
            f"{os.path.basename(path):<30} playback: worst dE {worst_de:5.1f}, "
            f"worst contrast {worst_cr:5.2f}:1"
            + ("   " + "; ".join(bad) if bad else ""))
        if bad:
            problems[name] = bad

    print(f"\n{len(PALETTES)} colour skins written. Add them to ../score.qrc.")
    if problems:
        print("\nplayback problems:")
        for n, bad in problems.items():
            print(f"  {n}: {'; '.join(bad)}")
        sys.exit(1)
