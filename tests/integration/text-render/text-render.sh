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
# after edge cases) + compare.py golden check against refs/llvmpipe for the
# stable subset (default / base / unicode).
#
#   check mode (default) : run once, analyze + golden compare (profile strict).
#   --update-refs        : run TWICE, accept refs only if both runs agree
#                          (compare.py --profile self) AND analyze.py passes.
#
# PASS = exit 0, no ASAN error, no JS CASE-ERROR, all assertions green.
# Self-serializes on flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"   # tests/integration/text-render -> repo root
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
OUT="${OUT:-/tmp/text-render}"
REFS="$HERE/refs/llvmpipe"
COMPARE="$HERE/../golden-render/compare.py"
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
# "default" is excluded from goldens while the off-screen-default bug makes it
# a blank frame (a blank golden would be meaningless).
GOLDEN=(base size-large unicode)

UPDATE=0
[ "${1:-}" = "--update-refs" ] && UPDATE=1

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }
python3 -c "import numpy, PIL, scipy" 2>/dev/null \
  || { echo "SKIP: python numpy/PIL/scipy missing"; exit 77; }

mkdir -p "$OUT" "$REFS"

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
        SCORE_FORCE_OFFSCREEN_WINDOW=Window QT_QPA_PLATFORM=offscreen \
        LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$dir/run.profraw" \
      timeout --foreground "$TIMEOUT" "$BIN" --no-gui --no-restore \
        --script "$HERE/text-cases.js" --wait 1 --autoplay >"$dir/run.log" 2>&1 &
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
    send /exit
    wait "$APP"; echo $? > "$dir/run.rc"
  ) 9>/tmp/score-harness.lock
}

check_run_health() { # dir -> appends to $FAILS
  local dir="$1" rc
  rc=$(cat "$dir/run.rc" 2>/dev/null || echo 97)
  [ "$rc" = 0 ] || FAILS+=" exit=$rc($dir)"
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
  for g in "${GOLDEN[@]}"; do
    if res=$(python3 "$COMPARE" "$OUT/A/$g.png" "$OUT/B/$g.png" --profile self); then
      cp "$OUT/A/$g.png" "$REFS/$g.png"
      echo "REF-UPDATED $g ($res)"
    else
      FAILS+=" UNSTABLE@$g($res)"
    fi
  done
else
  run_sequence "$OUT/run"
  check_run_health "$OUT/run"
  python3 "$HERE/analyze.py" "$OUT/run" || FAILS+=" ANALYZE"
  missing_refs=0
  for g in "${GOLDEN[@]}"; do
    if [ ! -f "$REFS/$g.png" ]; then
      echo "NOREF $g (run --update-refs)"; missing_refs=$((missing_refs+1)); continue
    fi
    if res=$(python3 "$COMPARE" "$REFS/$g.png" "$OUT/run/$g.png" --profile strict); then
      echo "GOLDEN $g PASS ($res)"
    else
      FAILS+=" GOLDEN@$g($res)"
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
