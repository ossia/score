#!/bin/sh
# Hardware probe: is a live PipeWire daemon reachable?
# Exit 0 = present, non-zero = absent (wrapper turns that into ctest SKIP 77).
set -u

sock="${PIPEWIRE_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}}/${PIPEWIRE_REMOTE:-pipewire-0}"
[ -S "$sock" ] || exit 1

# A stale socket with no daemon behind it must not pass: ping the core.
if command -v pw-cli >/dev/null 2>&1; then
  timeout 5 pw-cli info 0 >/dev/null 2>&1 || exit 1
fi

exit 0
