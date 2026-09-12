#!/bin/sh
# Run "$@" against a private X server instead of the developer's session.
#
# A GUI test maps real windows: on the developer's display they flash over
# whatever is on screen, steal focus, and -- for the tests that drive input --
# grab the pointer and the keyboard. A throwaway Xvfb makes all of that
# invisible, and a test that wedges with a grab held cannot lock the session.
#
# Vulkan still runs on the real GPU here, since the ICD does not go through the
# X server to enumerate devices. OpenGL does: it falls back to llvmpipe, which
# is slower but complete (GL 4.6 core).
#
# One server per test rather than one for the whole ctest run: with -j every
# test gets its own, nothing has to agree on a display number, and there is no
# server left behind when ctest is interrupted. Xvfb picks the number itself
# through -displayfd, so parallel tests never race for one.
#
# Set SCORE_TESTS_NO_XVFB=1 to run on the inherited display instead, which is
# what you want when watching a test to see what it actually draws.
set -u

if [ "${SCORE_TESTS_NO_XVFB:-0}" != "0" ] || ! command -v Xvfb >/dev/null 2>&1; then
  exec "$@"
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/score-xvfb.XXXXXX")
xpid=""
cleanup() {
  [ -n "$xpid" ] && kill "$xpid" 2>/dev/null
  rm -rf "$tmpdir"
}
trap cleanup EXIT INT TERM

: > "$tmpdir/display"
Xvfb -displayfd 3 -screen 0 1920x1080x24 -nolisten tcp \
  3>"$tmpdir/display" >"$tmpdir/xvfb.log" 2>&1 &
xpid=$!

disp=""
i=0
while [ "$i" -lt 200 ]; do
  disp=$(cat "$tmpdir/display" 2>/dev/null | tr -d '\n')
  case "$disp" in
    '' | *[!0-9]*) disp="" ;;
    *) break ;;
  esac
  kill -0 "$xpid" 2>/dev/null || break
  sleep 0.05
  i=$((i + 1))
done

if [ -z "$disp" ]; then
  echo "run-with-display.sh: Xvfb did not come up" >&2
  sed 's/^/  /' "$tmpdir/xvfb.log" >&2
  exit 1
fi

# WAYLAND_DISPLAY would otherwise win the platform auto-detection and put the
# windows straight back on the developer's compositor; XAUTHORITY holds cookies
# for their server, not this one.
unset WAYLAND_DISPLAY
unset XAUTHORITY
DISPLAY=":$disp"
export DISPLAY

# Xvfb has no DRI3, so GLX here is llvmpipe whatever the machine has. Say so:
# a test that measures render throughput cannot make its claim on a software
# rasteriser and should skip rather than fail. Vulkan is not affected.
SCORE_TESTS_SOFTWARE_GL=1
export SCORE_TESTS_SOFTWARE_GL

"$@"
rc=$?
exit "$rc"
