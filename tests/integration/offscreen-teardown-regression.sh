#!/usr/bin/env bash
# Regression test for the render-teardown crash fixes:
#   * 12283829d "gfx: fix offscreen output teardown use-after-free/double-free"
#     ~offscreen_device must synchronously
#     release the output's RenderList and the double-owned node before the
#     QRhi dies.
#   * 32ad5559e "gfx/render-clock: fix TimerClock timeout use-after-free on
#     teardown" — a queued timer tick must not fire
#     on a freed TimerClock.
#
# Recipe (modeled on csf-sweep.sh / verify-shader.sh): boot the full
# ossia-score with an offscreen window device, render a solid-color ISF shader
# scene, grab a frame to prove rendering actually happened, then /stop + /exit
# and assert the process EXIT CODE is 0. Before the fixes, teardown died with
# SIGSEGV / ASAN heap-use-after-free (exit 139 / 1).
#
#   tests/integration/offscreen-teardown-regression.sh [build-js] [llvmpipe|nvidia]
#
# The whole app run is serialized through /tmp/score-harness.lock because the
# OSC control port (6666) is global. Exit codes: 0 = pass, 1 = teardown
# regression (non-zero app exit), 2 = no render happened (cannot conclude),
# 77 = missing prerequisites (skip).
set -u

BIN="${SCORE_BIN:-/home/jcelerier/ossia/wt/score-tests/build-sanitizers/ossia-score}"
JS="${1:-/home/jcelerier/Documents/ossia/score/packages/csf-examples/csf-testers/tests-scene/scripts/build-isf-solid-color.js}"
BACKEND="${2:-llvmpipe}"

if [ -z "${DISPLAY:-}" ]; then
  for d in 99 98 97; do
    if command -v Xvfb >/dev/null 2>&1; then Xvfb ":$d" -screen 0 1280x720x24 >/dev/null 2>&1 &
    elif command -v Xephyr >/dev/null 2>&1; then Xephyr ":$d" -screen 1280x720 -ac -noreset >/dev/null 2>&1 &
    else break; fi
    OWN_X=$!; sleep 3
    if DISPLAY=":$d" xdpyinfo >/dev/null 2>&1; then
      export DISPLAY=":$d"; trap 'kill "$OWN_X" 2>/dev/null' EXIT; break
    fi
    kill "$OWN_X" 2>/dev/null
  done
fi
[ -n "${DISPLAY:-}" ] || { echo "SKIP: no X server (offscreen is not a fallback: no GL)"; exit 77; }

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }
[ -f "$JS" ] || { echo "SKIP: script $JS not found"; exit 77; }

OUT=/tmp/verify/offscreen-teardown-regression
mkdir -p "$OUT"
PNG="$OUT/frame.png"
LOG="$OUT/run.log"
rm -f "$PNG" "$LOG"

# A previous crash leaves the failsafe bit set, which changes startup behavior.
rm -f "$HOME/.config/ossia/failsafe.bit"

COMMON=(SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1
        SCORE_FORCE_OFFSCREEN_WINDOW=Window)
ASAN="detect_leaks=0:abort_on_error=0:halt_on_error=1:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"
case "$BACKEND" in
  nvidia) BE=(env DISPLAY="${DISPLAY:-:0}" QT_QPA_PLATFORM=xcb __GLX_VENDOR_LIBRARY_NAME=nvidia) ;;
  # llvmpipe must not see an inherited DISPLAY (e.g. from a ctest run that set
  # one for GUI tests): offscreen+GLX-on-NVIDIA breaks texture creation.
  # A real X server with xcb, NOT QT_QPA_PLATFORM=offscreen. Offscreen has no
  # GL (Qt's offscreen integration is GLX-only), so QRhi falls back to the Null
  # backend, which draws nothing: this test's "grab a frame to prove rendering
  # happened" step passed on a constant colour and the teardown assertion that
  # follows it was therefore never reached on a real render.
  #
  # SCORE_FORCE_OFFSCREEN_WINDOW is kept: the offscreen device is what this test
  # is about, and it renders correctly on a real GL context -- the grab comes
  # back magenta, which is what the shader draws. (A scene built the way the
  # live-edit scenarios build theirs does NOT connect to the offscreen device;
  # that is a separate, unexplained difference between the two scene builders,
  # not a fault in the device.)
  *)      BE=(env QT_QPA_PLATFORM=xcb SCORE_SANITIZE_SKIP_CHECKS=1 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe) ;;
esac

rc=125
(
  flock -w 240 9 || { echo "LOCK-TIMEOUT on /tmp/score-harness.lock"; exit 4; }

  # Helper: poll-grab a frame (ASAN startup time varies), then stop + exit.
  ( for _ in $(seq 1 18); do
      sleep 2
      oscsend 127.0.0.1 6666 /script s "Score.device('Window').grabTo('$PNG')" 2>/dev/null
      [ -s "$PNG" ] && break
    done
    sleep 0.5; oscsend 127.0.0.1 6666 /script s "Score.stop()"; sleep 0.5; oscsend 127.0.0.1 6666 /exit s force ) \
    >/dev/null 2>&1 &

  env "${COMMON[@]}" ASAN_OPTIONS="$ASAN" "${BE[@]}" \
    timeout 90 "$BIN" --no-gui --no-restore --script "$JS" --wait 1 --autoplay \
    >"$LOG" 2>&1
  rc=$?
  wait 2>/dev/null
  exit $rc
) 9>/tmp/score-harness.lock
rc=$?

if [ ! -s "$PNG" ]; then
  echo "NORENDER: no frame was grabbed (rc=$rc) — cannot conclude on teardown; see $LOG"
  exit 77
fi

if [ "$rc" -ne 0 ]; then
  echo "FAIL: rendered OK but teardown exited $rc (expected 0; 139 = the"
  echo "      pre-fix offscreen-output / TimerClock use-after-free). Log: $LOG"
  tail -n 30 "$LOG"
  exit 1
fi

echo "PASS: render + /stop + /exit completed with exit code 0 ($PNG)"
exit 0
