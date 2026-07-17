#!/usr/bin/env bash
# Timeline-driven scenario runner.
#
#   tests/integration/timeline-scenarios/timeline-scenario.sh
#
# Plays scenario-ramp.js headless on llvmpipe, then for each probe position T:
#   OSC /transport <T ms>  ->  Score.pause()  ->  settle  ->  grabTo(png)
#   ->  assert frame mean == T / RAMP_MS within +-TOL  ->  Score.resume().
# This asserts rendered output AND execution state at specific timeline
# positions, not just a static frame: the automation's value at T is the
# state, and it is observable as the pixel mean (see ramp-level.fs).
#
# PASS = exit 0, no ASAN error, every probe within tolerance, means strictly
# increasing across probes (proves /transport really repositions).
# Runs under flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"  # tests/integration/timeline-scenarios -> repo root
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
OUT="${OUT:-/tmp/timeline-scenarios}"
OSC=6666
RAMP_MS=10000
POSITIONS=(${POSITIONS:-2000 5000 8000})
# One-directional value band: the grab lands ahead of T by the settle time, so
# mean is expected in [T/RAMP - LO, T/RAMP + HI]. HI covers seek+settle drift.
TOL_LO="${TOL_LO:-0.04}"
TOL_HI="${TOL_HI:-0.15}"
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"

# Prerequisites -> ctest SKIP (return 77) rather than a hard failure.
# (macOS lacks flock/timeout/oscsend/convert; the harness is Linux-oriented.)
command -v flock   >/dev/null || { echo "SKIP: flock not found";       exit 77; }
command -v timeout >/dev/null || { echo "SKIP: timeout not found";     exit 77; }
command -v oscsend >/dev/null || { echo "SKIP: oscsend not found";     exit 77; }
command -v convert >/dev/null || { echo "SKIP: ImageMagick not found"; exit 77; }
[ -x "$BIN" ]                 || { echo "SKIP: $BIN not built";        exit 77; }

mkdir -p "$OUT"
rm -f "$OUT"/ramp-init.score "$OUT"/ramp-*.png "$OUT"/ramp.log "$OUT"/ramp.rc \
      "$HOME/.config/ossia/failsafe.bit"

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
mean_of() { convert "$1" -format '%[fx:mean]' info: 2>/dev/null || echo -1; }

(
  flock -w 900 9 || { echo 98 > "$OUT/ramp.rc"; exit 0; }
  env -u DISPLAY XDG_CONFIG_HOME="$CFG" \
      SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
      SCORE_FORCE_OFFSCREEN_WINDOW=Window QT_QPA_PLATFORM=offscreen \
      LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
      ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/ramp.profraw" \
    timeout --foreground 300 "$BIN" --no-gui --no-restore \
      --script "$HERE/scenario-ramp.js" --wait 1 --autoplay >"$OUT/ramp.log" 2>&1 &
  APP=$!

  for _ in $(seq 1 120); do [ -s "$OUT/ramp-init.score" ] && break; sleep 1; done
  if [ ! -s "$OUT/ramp-init.score" ]; then
    echo "no readiness marker — startup failed (see $OUT/ramp.log)" >&2
    kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$OUT/ramp.rc"; exit 0
  fi
  sleep 3   # let autoplay actually start the engine

  # Playback stays RUNNING: /transport repositions the playhead and the running
  # engine re-renders the automation value at the new spot. (A paused seek does
  # NOT re-render — no tick fires while paused — so it freezes the last frame.)
  # The grab therefore lands slightly AHEAD of T by the settle time; the verdict
  # uses a one-directional drift band + monotonicity rather than exact equality.
  # Frame-exact stepping needs the offline/driven clock (render-clock-RFC, P2).
  for T in "${POSITIONS[@]}"; do
    png="$OUT/ramp-$T.png"
    rm -f "$png"
    send /transport f "$T"; sleep 0.4   # reposition, minimal settle
    for _ in $(seq 1 10); do
      send /script s "Score.device('Window').grabTo('$png')"
      sleep 0.6; [ -s "$png" ] && break
    done
  done

  send /stop; sleep 0.5
  send /exit
  wait "$APP"; echo $? > "$OUT/ramp.rc"
) 9>/tmp/score-harness.lock

# ---------------- verdict ----------------
fails=""
rc=$(cat "$OUT/ramp.rc" 2>/dev/null || echo 97)
[ "$rc" = 0 ] || fails+=" exit=$rc"
grep -q "ERROR: AddressSanitizer" "$OUT/ramp.log" 2>/dev/null && fails+=" ASAN"
grep -q "SCENARIO-ERROR" "$OUT/ramp.log" 2>/dev/null && fails+=" JSERR"

# LC_ALL=C: force '.' decimals in awk (a comma locale breaks numeric compares).
prev=-1
report=""
for T in "${POSITIONS[@]}"; do
  png="$OUT/ramp-$T.png"
  if [ ! -s "$png" ]; then fails+=" NORENDER@$T"; continue; fi
  m=$(mean_of "$png")
  exp=$(LC_ALL=C awk "BEGIN{printf \"%.4f\", $T/$RAMP_MS}")
  report+=" T=${T}ms mean=$m expect~$exp;"
  LC_ALL=C awk "BEGIN{exit !($m >= $exp-$TOL_LO && $m <= $exp+$TOL_HI)}" \
    || fails+=" OFF@$T(mean=$m expect~$exp band[-$TOL_LO,+$TOL_HI])"
  # distinct + increasing: each position must render clearly ahead of the last
  # (proves the seek actually repositioned, not a frozen frame).
  LC_ALL=C awk "BEGIN{exit !($m > $prev + 0.1)}" || fails+=" NOT-ADVANCED@$T(mean=$m prev=$prev)"
  prev=$m
done

if [ -z "$fails" ]; then
  echo "timeline-scenario ramp PASS $report"
else
  echo "timeline-scenario ramp FAIL:$fails  ($report log=$OUT/ramp.log)"; exit 1
fi
