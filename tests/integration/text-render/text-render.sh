#!/usr/bin/env bash
# TEXT node render validation.
#
#   tests/integration/text-render/text-render.sh [--update-refs]
#
# One headless llvmpipe app run plays text-cases.js (Window + Gfx::Text
# process). The FIRST grab captures the process DEFAULTS (visibility
# regression check), then each case is applied live over OSC /script
# (setCase(name) mutates the Text controls in the persistent console JS
# engine) and the frame is grabbed. Verdict = analyze.py VALUE assertions
# (pixel coverage, bbox ordering across point sizes, centroid movement for
# position, channel dominance for color, blank for empty string, recovery
# after edge cases) + compare.py golden check against refs/ for the
# stable subset (default / base / unicode). The goldens are backend-independent
# (one per case, not one per backend); compare.py owns the tolerance.
#
#   check mode (default) : run once, analyze + golden compare (profile shared).
#                          On FAIL the golden, the actual and a diff image are
#                          written to $OUT/diff/, named after the case.
#   --update-refs        : run TWICE, accept refs only if both runs agree
#                          (compare.py --profile self) AND analyze.py passes.
#                          An update that would move an EXISTING golden outside
#                          the shared tolerance is refused as REF-CONFLICT.
#   --rebless            : allow --update-refs to replace a golden the new
#                          render does not match. Deliberate, and the picture
#                          must be looked at.
#
# ---------------------------------------------------------------------------
# THE RENDER ENVIRONMENT IS THE TEST.
#
# This harness used to launch under QT_QPA_PLATFORM=offscreen. Qt's offscreen
# integration exposes OpenGL only through GLX, so with DISPLAY unset there is no
# GL at all: QRhi fell back to the Null backend, which fills every texture with
# Qt::yellow (qrhinull.cpp), and every "rendered" frame this suite ever graded
# was a solid (255,255,0) rectangle. score itself printed the diagnosis on every
# run ("NULL RHI BACKEND ... use a real X server with QT_QPA_PLATFORM=xcb") and
# nothing read it. So: a real X server (our own Xvfb when we can start one),
# QT_QPA_PLATFORM=xcb, and __GLX_VENDOR_LIBRARY_NAME=mesa alongside
# LIBGL_ALWAYS_SOFTWARE=1 -- without the vendor pin libglvnd hands us the NVIDIA
# GPU and LIBGL_ALWAYS_SOFTWARE is silently ignored (measured in
# golden-render.sh, which has used exactly this line all along and passes).
#
# And the backend is asserted, not assumed: the run is failed outright if QRhi
# does not report llvmpipe, or if score reports the Null backend. A suite that
# cannot say what drew its pixels cannot say anything about them.
# ---------------------------------------------------------------------------
#
# PASS = exit 0, no ASAN error, no JS CASE-ERROR, all assertions green.
# Self-serializes on flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"   # tests/integration/text-render -> repo root
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
OUT="${OUT:-/tmp/text-render}"
# One golden per case, shared by every backend -- see the compare.py docstring
# for why the per-backend trees were collapsed and what tolerance replaced them.
REFS="$HERE/refs"
COMPARE="$HERE/../golden-render/compare.py"
DIFFDIR="$OUT/diff"
OSC=6666
TIMEOUT="${TIMEOUT:-420}"
SETTLE="${SETTLE:-1.2}"
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"

# Grab order. "default" is the untouched initial state and MUST come first;
# "default-pos0" must precede any case that changes the text (it keeps the
# process-default string and only repositions it on screen — the isolation
# probe for the off-screen-default bug). tofu/empty precede base-again so the
# last case proves clean recovery.
CASES=(default-pos0 base size-small size-large font-sans color-red
       pos-left pos-right pos-down scale-half
       unicode cjk longstr tofu empty base-again)
# "default" is not a golden of its own: with the off-screen-default fix in
# place it renders, and its frame is byte-identical to default-pos0 (both are
# the process defaults at position (0,0)), so a second copy would be graded
# twice and mean once. The hard default-visible assertion in analyze.py is what
# guards it.
GOLDEN=(base size-large unicode)

UPDATE=0
REBLESS=0
for a in "$@"; do
  case "$a" in
    --update-refs) UPDATE=1 ;;
    --rebless) REBLESS=1 ;;
    *) echo "unknown argument: $a"; exit 2 ;;
  esac
done

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }
python3 -c "import numpy, PIL, scipy" 2>/dev/null \
  || { echo "SKIP: python numpy/PIL/scipy missing"; exit 77; }

mkdir -p "$OUT" "$REFS" "$DIFFDIR"

# ---- a real X server, ours if we can have one -------------------------------
# Preferring our own Xvfb over $DISPLAY keeps the run headless and reproducible
# on CI, and keeps it off the developer's live session. Falling back to an
# existing $DISPLAY keeps it runnable on a box without Xvfb. With no display at
# all we SKIP rather than silently degrade: the Null-backend frames this suite
# used to grade are exactly what "degrade quietly" produces.
XVFB_PID=""
DISP=""
if command -v Xvfb >/dev/null && command -v xdpyinfo >/dev/null; then
  for n in $(seq 90 120); do
    [ -e "/tmp/.X11-unix/X$n" ] && continue
    Xvfb ":$n" -screen 0 1920x1080x24 +extension GLX >"$OUT/xvfb.log" 2>&1 &
    XVFB_PID=$!
    for _ in $(seq 1 20); do
      DISPLAY=":$n" xdpyinfo >/dev/null 2>&1 && { DISP=":$n"; break; }
      kill -0 "$XVFB_PID" 2>/dev/null || break
      sleep 0.25
    done
    [ -n "$DISP" ] && break
    kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null; XVFB_PID=""
  done
fi
if [ -z "$DISP" ] && [ -n "${DISPLAY:-}" ] \
   && { ! command -v xdpyinfo >/dev/null || xdpyinfo >/dev/null 2>&1; }; then
  DISP="$DISPLAY"
fi
[ -n "$DISP" ] || { echo "SKIP: no X display (Xvfb unavailable and \$DISPLAY unusable)"; exit 77; }
cleanup_xvfb() { [ -n "$XVFB_PID" ] && { kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null; }; }
trap cleanup_xvfb EXIT
echo "display=$DISP$([ -n "$XVFB_PID" ] && echo ' (own Xvfb)' || echo ' (inherited)')"

# The renderer QRhi must report, and the phrase score prints when it got none.
EXPECT_RENDERER='llvmpipe'
NULL_RHI='NULL RHI BACKEND'

# Hermetic config home, GraphicsApi pinned to OpenGL (user conf may say Vulkan).
CFG="$OUT/config-home"; mkdir -p "$CFG/ossia"
python3 - "$HOME/.config/ossia/score.conf" "$CFG/ossia/score.conf" <<'EOF'
import re, sys, pathlib
src, dst = sys.argv[1], sys.argv[2]
try: text = pathlib.Path(src).read_text()
except OSError: text = ""
if "[score_plugin_gfx]" not in text:
    text += "\n[score_plugin_gfx]\nGraphicsApi=OpenGL\n"
elif re.search(r"^GraphicsApi=.*$", text, re.M):
    text = re.sub(r"^GraphicsApi=.*$", "GraphicsApi=OpenGL", text, flags=re.M)
else:
    text = text.replace("[score_plugin_gfx]", "[score_plugin_gfx]\nGraphicsApi=OpenGL")
pathlib.Path(dst).write_text(text)
EOF

# text-cases.js writes its readiness marker and its saved documents through a
# single OUT_DIR constant. That constant was the literal "/tmp/text-render"
# while this script honoured $OUT, so under ctest -- which sets OUT into the
# build tree -- the marker landed somewhere nobody was watching and every run
# died at the readiness gate with exit 97, having graded nothing at all. Point
# the JS at the same directory the shell uses.
SCRIPT_JS="$OUT/text-cases.js"
sed "s#^var OUT_DIR .*#var OUT_DIR     = \"$OUT\";#" "$HERE/text-cases.js" > "$SCRIPT_JS"
grep -q "^var OUT_DIR     = \"$OUT\";" "$SCRIPT_JS" \
  || { echo "SKIP: could not point text-cases.js at $OUT"; exit 77; }

send() { oscsend 127.0.0.1 $OSC "$@" 2>/dev/null; }

grab() { # png -> 0 iff file written
  local png="$1"
  rm -f "$png"
  for _ in $(seq 1 12); do
    send /script s "Score.device('Window').grabTo('$png')"
    sleep 0.6; [ -s "$png" ] && return 0
  done
  return 1
}

run_sequence() { # outdir -> writes <outdir>/<case>.png + run.log + run.rc
  local dir="$1"
  mkdir -p "$dir"
  rm -f "$dir"/*.png "$dir/run.log" "$dir/run.rc" "$OUT/text-init.score" \
        "$HOME/.config/ossia/failsafe.bit"
  (
    flock -w 900 9 || { echo 98 > "$dir/run.rc"; exit 0; }
    env -u DISPLAY XDG_CONFIG_HOME="$CFG" \
        SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
        SCORE_FORCE_OFFSCREEN_WINDOW=Window \
        DISPLAY="$DISP" QT_QPA_PLATFORM=xcb \
        __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        QT_LOGGING_RULES='qt.rhi.general=true' \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$dir/run.profraw" \
      timeout --foreground "$TIMEOUT" "$BIN" --no-gui --no-restore \
        --script "$SCRIPT_JS" --wait 1 --autoplay >"$dir/run.log" 2>&1 &
    local APP=$!

    local ok=0
    for _ in $(seq 1 120); do [ -s "$OUT/text-init.score" ] && { ok=1; break; }; sleep 1; done
    if [ "$ok" = 0 ]; then
      echo "no readiness marker — startup failed (see $dir/run.log)" >&2
      kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$dir/run.rc"; exit 0
    fi
    sleep 3   # let autoplay start the engine and the first frame render

    grab "$dir/default.png" || echo "GRAB-FAIL default" >> "$dir/run.log"
    for c in "${CASES[@]}"; do
      send /script s "setCase('$c')"
      sleep "$SETTLE"
      grab "$dir/$c.png" || echo "GRAB-FAIL $c" >> "$dir/run.log"
    done

    # Save before exiting: a dirty document under the offscreen QPA aborts in
    # the closeDocument "save changes?" QMessageBox (qt_assert, exit 134).
    send /script s "finalizeRun()"
    sleep 1
    send /stop; sleep 0.5
    send /exit s force
    wait "$APP"; echo $? > "$dir/run.rc"
  ) 9>/tmp/score-harness.lock
}

check_run_health() { # dir -> appends to $FAILS
  local dir="$1" rc renderer
  rc=$(cat "$dir/run.rc" 2>/dev/null || echo 97)
  [ "$rc" = 0 ] || FAILS+=" exit=$rc($dir)"

  # Backend identity, asserted. The Null backend is not a degraded render, it
  # is no render: it hands back a Qt::yellow fill that satisfies most of a
  # value assertion suite. Refusing to grade a run we cannot attribute is the
  # whole point.
  if grep -q "$NULL_RHI" "$dir/run.log" 2>/dev/null; then
    FAILS+=" NULL-RHI($dir)"
    echo "  backend: NULL RHI — nothing was rendered, see $dir/run.log"
  else
    renderer=$(grep -m1 'qt\.rhi\.general: OpenGL VENDOR' "$dir/run.log" 2>/dev/null \
               | sed 's/^.*qt\.rhi\.general: //')
    printf '%s\n' "$renderer" > "$dir/renderer"
    if [ -z "$renderer" ]; then
      FAILS+=" NO-RENDERER-LINE($dir)"
    elif ! printf '%s' "$renderer" | grep -q "$EXPECT_RENDERER"; then
      FAILS+=" WRONG-BACKEND($dir)"
      echo "  backend: expected /$EXPECT_RENDERER/, got: $renderer"
    else
      echo "  backend: $renderer"
    fi
  fi

  grep -q "ERROR: AddressSanitizer" "$dir/run.log" 2>/dev/null && FAILS+=" ASAN($dir)"
  grep -q "CASE-ERROR\|SCENARIO-ERROR" "$dir/run.log" 2>/dev/null && FAILS+=" JSERR($dir)"
  grep -q "GRAB-FAIL" "$dir/run.log" 2>/dev/null && FAILS+=" $(grep -o 'GRAB-FAIL [a-z-]*' "$dir/run.log" | tr ' ' '@' | tr '\n' ' ')"
}

FAILS=""
if [ "$UPDATE" = 1 ]; then
  run_sequence "$OUT/A"
  run_sequence "$OUT/B"
  check_run_health "$OUT/A"; check_run_health "$OUT/B"
  python3 "$HERE/analyze.py" "$OUT/A" || FAILS+=" ANALYZE(A)"
  if [ -z "$FAILS" ]; then
    for g in "${GOLDEN[@]}"; do
      if ! res=$(python3 "$COMPARE" "$OUT/A/$g.png" "$OUT/B/$g.png" --profile self); then
        FAILS+=" UNSTABLE@$g($res)"
        continue
      fi
      # Two agreeing runs prove determinism, not correctness. If a golden
      # already exists and the new render does not match it, something moved,
      # and quietly writing over the reference is how the thing that moved
      # stops being visible. Say so and keep what is committed unless the
      # replacement was asked for by name.
      if [ -f "$REFS/$g.png" ] && [ "$REBLESS" = 0 ] \
         && ! conflict=$(python3 "$COMPARE" "$REFS/$g.png" "$OUT/A/$g.png" --profile shared); then
        echo "REF-CONFLICT $g — the new render does not match the committed golden."
        echo "    $conflict"
        echo "    Look at the picture. Pass --rebless if the new one is right."
        FAILS+=" REF-CONFLICT@$g"
        continue
      fi
      cp "$OUT/A/$g.png" "$REFS/$g.png"
      echo "REF-UPDATED $g ($res)"
    done
  else
    # A deterministically broken render agrees with itself; without this
    # gate it would become a permanent reference.
    echo "REFS NOT UPDATED:$FAILS"
  fi
else
  run_sequence "$OUT/run"
  check_run_health "$OUT/run"
  python3 "$HERE/analyze.py" "$OUT/run" || FAILS+=" ANALYZE"
  missing_refs=0
  for g in "${GOLDEN[@]}"; do
    if [ ! -f "$REFS/$g.png" ]; then
      echo "NOREF $g (run --update-refs)"; missing_refs=$((missing_refs+1)); continue
    fi
    if res=$(python3 "$COMPARE" "$REFS/$g.png" "$OUT/run/$g.png" \
               --profile shared --diff-dir "$DIFFDIR" --name "$g"); then
      echo "GOLDEN $g PASS ($res)"
    else
      FAILS+=" GOLDEN@$g($res)"
      echo "GOLDEN $g FAIL ($res)"
      echo "    artifacts=$DIFFDIR/$g.{golden,actual,diff}.png"
    fi
  done
  # No refs at all (fresh checkout on another rig): golden part SKIPs, value
  # assertions above still gate.
  if [ "$missing_refs" = "${#GOLDEN[@]}" ] && [ -z "$FAILS" ]; then
    echo "text-render PASS (value assertions only; no golden refs present)"
    exit 0
  fi
  [ "$missing_refs" = 0 ] || FAILS+=" NOREF=$missing_refs"
fi

if [ -z "$FAILS" ]; then
  echo "text-render PASS$([ "$UPDATE" = 1 ] && echo ' (refs updated)')"
else
  echo "text-render FAIL:$FAILS  (out=$OUT)"; exit 1
fi
