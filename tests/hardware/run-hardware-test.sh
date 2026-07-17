#!/bin/sh
# Probe-then-exec wrapper for hardware-gated ctest entries.
#
#   run-hardware-test.sh '<probe shell command>' -- <harness> [args...]
#
# Runs the probe with `sh -c`; when it fails the required device is absent on
# this machine and we exit 77, which the score_add_hardware_test() helper maps
# to a ctest SKIP via SKIP_RETURN_CODE. When the probe succeeds we exec() the
# real harness so its exit code (and signals) reach ctest unchanged.
set -u

if [ $# -lt 3 ] || [ "$2" != "--" ]; then
  echo "usage: $0 '<probe shell command>' -- <harness> [args...]" >&2
  exit 2
fi

probe=$1
shift 2

if ! sh -c "$probe" >/dev/null 2>&1; then
  echo "SKIP: hardware probe failed: $probe" >&2
  exit 77
fi

# Headless fallback: the roundtrip harnesses are GUI-class Qt apps (they build
# a real render graph). On a rig DISPLAY is set; on a display-less box that
# still has the device (e.g. PipeWire under ssh), fall back to the offscreen
# platform instead of dying in the xcb platform plugin.
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] \
   && [ -z "${QT_QPA_PLATFORM:-}" ]; then
  export QT_QPA_PLATFORM=offscreen
fi

exec "$@"
