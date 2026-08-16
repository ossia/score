#!/usr/bin/env bash
# Acceptance test for deterministic frame stepping.
#
# Renders the SAME tester to the SAME frame index in two separate processes and
# requires the two PNGs to be byte-identical. Two processes rather than two
# grabs in one, because anything cached in the process would hide exactly the
# nondeterminism this is looking for.
#
# If this fails, golden references cannot be pinned: frame N is not a stable
# picture, and every stored reference is a snapshot of one particular run.
#
#   frame-determinism.sh [--score PATH] [--case NAME] [--frame N]
#
# Exit 0 iff the two renders agree.
set -uo pipefail

SCORE="${OSSIA_SCORE:-}"
# Animated on purpose: this tester draws TIME, TIMEDELTA, PROGRESS and
# FRAMEINDEX as bars, so it fails if any of them still follows a wall clock. A
# static case would pass whether or not the step clock works.
CASE="build-isf-time-uniforms"
FRAME=30
OUT="${TMPDIR:-/tmp}/score-frame-determinism.$$"

while [ $# -gt 0 ]; do
  case "$1" in
    --score) SCORE="$2"; shift 2 ;;
    --case)  CASE="$2";  shift 2 ;;
    --frame) FRAME="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$SCORE" ] || { echo "FATAL: set OSSIA_SCORE or pass --score"; exit 2; }
[ -x "$SCORE" ] || { echo "FATAL: not executable: $SCORE"; exit 2; }

mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT

# The tester scripts resolve their own paths through <LIBRARY>:, so the only
# thing this needs to know is the case name.
render() {
  local n="$1"
  # Score.play() first: the gfx nodes only exist while the score executes, so a
  # grab on a stopped document finds nothing wired to the Window device.
  cat > "$OUT/run$n.js" <<JS
eval(Score.readFile("<LIBRARY>:/packages/csf-examples/csf-testers/tests-scene/scripts/${CASE}.js"));
Score.play();
Score.device('Window').grabFrame(${FRAME}, "${OUT}/frame$n.png");
JS
  # Without this the Window device opens a real window and grabTo falls back to
  # QScreen::grabWindow, which reads the framebuffer at the window's geometry --
  # i.e. the desktop, screensaver included. Checked again below, because a
  # screenshot of a static desktop is byte-stable and would "pass".
  SCORE_FORCE_OFFSCREEN_WINDOW=Window \
  SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 DISPLAY="${DISPLAY:-:0}" \
    timeout 120 "$SCORE" --no-gui --no-restore --script "$OUT/run$n.js" --wait 2 --autoplay \
    > "$OUT/run$n.log" 2>&1
  return $?
}

echo "== rendering $CASE frame $FRAME, twice, in separate processes =="
render 1; rc1=$?
render 2; rc2=$?

fail=0
for n in 1 2; do
  if [ ! -s "$OUT/frame$n.png" ]; then
    echo "FAIL: run $n produced no frame (exit $([ $n = 1 ] && echo $rc1 || echo $rc2))"
    sed 's/^/    /' "$OUT/run$n.log" | tail -20
    fail=1
  fi
done
[ $fail -eq 1 ] && exit 1

# A screen grab of a quiet desktop is byte-identical between two runs, so this
# has to be fatal rather than cosmetic: it is the difference between comparing
# renders and comparing wallpaper.
for n in 1 2; do
  if grep -q "capturing the SCREEN" "$OUT/run$n.log"; then
    echo "FAIL: run $n grabbed the screen instead of the render."
    echo "  The offscreen device was not selected -- SCORE_FORCE_OFFSCREEN_WINDOW"
    echo "  must name the Window device, and this build must honour it."
    exit 1
  fi
  if grep -q "nothing rendered into" "$OUT/run$n.log"; then
    echo "FAIL: run $n rendered nothing -- no process is wired to the Window device."
    grep "nothing rendered into" "$OUT/run$n.log" | sed 's/^/    /'
    exit 1
  fi
done

# Identical blank frames are identical. Without this, a case that renders
# nothing at all is the easiest way to pass a determinism test, and the result
# would say the mechanism works when it has not been exercised.
blank=$(python3 - "$OUT/frame1.png" <<'PY' 2>/dev/null
import sys
try:
    from PIL import Image
    print(len(set(Image.open(sys.argv[1]).convert('RGB').getdata())))
except Exception:
    print("?")
PY
)
case "$blank" in
  "?") echo "NOTE: cannot check for blankness (no PIL); result is weaker than it looks" ;;
  1)   echo "FAIL: frame $FRAME is a single flat colour -- nothing was rendered."
       echo "  Two blank frames match trivially; this proves nothing about determinism."
       exit 1 ;;
  *)   echo "  frame has $blank distinct colours (not blank)" ;;
esac

if cmp -s "$OUT/frame1.png" "$OUT/frame2.png"; then
  echo "PASS: both runs produced byte-identical frames ($(stat -c%s "$OUT/frame1.png") bytes)"
  exit 0
fi

echo "FAIL: frame $FRAME differs between two runs of the same case."
echo "  run 1: $(stat -c%s "$OUT/frame1.png") bytes  md5 $(md5sum < "$OUT/frame1.png" | cut -d' ' -f1)"
echo "  run 2: $(stat -c%s "$OUT/frame2.png") bytes  md5 $(md5sum < "$OUT/frame2.png" | cut -d' ' -f1)"
echo "  Frame stepping is not deterministic yet; golden references cannot be pinned."
cp "$OUT/frame1.png" "$OUT/frame2.png" "${TMPDIR:-/tmp}/" 2>/dev/null && \
  echo "  copies kept in ${TMPDIR:-/tmp}/frame1.png and frame2.png"
exit 1
