#!/usr/bin/env bash
# Provision-then-exec wrapper for media tests that are ALLOWED TO ASSUME a
# capable host: gstreamer, ffmpeg and a live PipeWire daemon are requirements,
# not conditions. Anything missing is a HARD FAILURE (exit 1), never a SKIP --
# the point of this runner is that "the device was absent" stops being an
# explanation for an untested code path.
#
#   with-virtual-media.sh [--video] [--media] -- <harness> [args...]
#
#   --video   publish a PipeWire Video/Source node ($SCORE_TEST_PW_VIDEO_NODE)
#   --media   generate H.264 + raw test clips  ($SCORE_TEST_MEDIA_DIR)
#
# Everything provisioned here is torn down on exit, including on failure.
set -u

want_video=0; want_media=0
while [ $# -gt 0 ]; do
  case "$1" in
    --video) want_video=1; shift;;
    --media) want_media=1; shift;;
    --) shift; break;;
    *) echo "with-virtual-media: unexpected arg '$1'" >&2; exit 2;;
  esac
done
[ $# -ge 1 ] || { echo "usage: $0 [--video] [--media] -- <harness> [args...]" >&2; exit 2; }

# Default is STRICT: a host running these tests is expected to have the media
# stack, so a missing piece is a failure and not a skip -- otherwise "the device
# was absent" silently explains away an untested code path forever. Hosts that
# genuinely cannot provide it (a minimal CI container) set
# SCORE_MEDIA_TESTS_OPTIONAL=1 and get the old ctest SKIP instead.
die() {
  if [ "${SCORE_MEDIA_TESTS_OPTIONAL:-0}" = 1 ]; then
    echo "with-virtual-media: SKIP (optional mode): $*" >&2
    exit 77
  fi
  echo "with-virtual-media: FATAL: $*" >&2
  echo "with-virtual-media: set SCORE_MEDIA_TESTS_OPTIONAL=1 to downgrade to a skip" >&2
  exit 1
}

for tool in gst-launch-1.0 ffmpeg; do
  command -v "$tool" >/dev/null 2>&1 || die "$tool not on PATH (this runner requires it)"
done
gst-inspect-1.0 pipewiresink >/dev/null 2>&1 || die "gstreamer has no pipewiresink element"

sock="${PIPEWIRE_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}}/${PIPEWIRE_REMOTE:-pipewire-0}"
[ -S "$sock" ] || die "no PipeWire socket at $sock"
if command -v pw-cli >/dev/null 2>&1; then
  timeout 5 pw-cli info 0 >/dev/null 2>&1 || die "PipeWire socket present but the core does not answer"
fi

tmp=$(mktemp -d)
pids=""
cleanup() {
  for p in $pids; do kill "$p" 2>/dev/null; done
  for p in $pids; do wait "$p" 2>/dev/null; done
  rm -rf "$tmp"
}
trap cleanup EXIT INT TERM

if [ "$want_video" = 1 ]; then
  node="score-test-vsrc-$$"
  gst-launch-1.0 -q videotestsrc is-live=true pattern=smpte \
    ! video/x-raw,format=RGBA,width=320,height=240,framerate=30/1 \
    ! pipewiresink mode=provide \
      stream-properties="p,media.class=Video/Source,node.name=$node" \
    > "$tmp/vsrc.log" 2>&1 &
  pids="$pids $!"

  ok=0
  for _ in 1 2 3 4 5 6 7 8 9 10; do
    sleep 1
    if timeout 8 pw-dump 2>/dev/null | grep -q "$node"; then ok=1; break; fi
  done
  [ "$ok" = 1 ] || { echo "--- gst log ---" >&2; cat "$tmp/vsrc.log" >&2; \
                     die "virtual PipeWire video node '$node' never appeared"; }
  export SCORE_TEST_PW_VIDEO_NODE="$node"
fi

if [ "$want_media" = 1 ]; then
  ffmpeg -nostdin -loglevel error -y -f lavfi -i testsrc=size=320x240:rate=30:duration=2 \
    -pix_fmt yuv420p -c:v libx264 -preset ultrafast "$tmp/h264.mp4" \
    || die "ffmpeg could not produce the H.264 clip"
  ffmpeg -nostdin -loglevel error -y -f lavfi -i testsrc=size=160x120:rate=25:duration=1 \
    -pix_fmt yuv420p -c:v rawvideo -f nut "$tmp/raw.nut" \
    || die "ffmpeg could not produce the raw clip"
  export SCORE_TEST_MEDIA_DIR="$tmp"
fi

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] \
   && [ -z "${QT_QPA_PLATFORM:-}" ]; then
  export QT_QPA_PLATFORM=offscreen
fi

"$@"
rc=$?
exit $rc
