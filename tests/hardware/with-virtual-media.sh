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

  # Per-pixel-format clips. rawvideo-in-NUT is the only container that carries
  # an arbitrary pix_fmt through untouched -- every other muxer silently
  # converts, which would make the decoder tests assert ffmpeg's conversion
  # instead of score's. Named fmt-<pixfmt>-<W>x<H>.nut and discovered by name,
  # so a format added here needs no test change.
  #
  # ODD dimensions are deliberate: a 4:2:0 chroma plane of an odd-sized frame is
  # ceil(w/2) x ceil(h/2), and every consumer that computes w/2 is wrong by a
  # row or a column there.
  #
  # The explicit scale filter is load-bearing: lavfi sources silently round an
  # odd requested size DOWN to even, so `testsrc2=size=65x33` alone yields a
  # 64x32 clip and the odd-dimension axis quietly disappears. The size is read
  # back with ffprobe afterwards and a mismatch is fatal, so a clip can never
  # again claim in its name a geometry it does not have.
  gen_raw() {  # gen_raw <pixfmt> <W> <H> [frames]
    local fmt="$1" w="$2" h="$3" n="${4:-8}"
    local out="$tmp/fmt-${fmt}-${w}x${h}.nut"
    ffmpeg -nostdin -loglevel error -y \
      -f lavfi -i "testsrc2=size=$((w * 2))x$((h * 2)):rate=25" \
      -vf "scale=${w}:${h}" -frames:v "$n" \
      -pix_fmt "$fmt" -c:v rawvideo -f nut "$out" \
      || die "ffmpeg could not produce a $fmt ${w}x${h} clip"
    if command -v ffprobe >/dev/null 2>&1; then
      got=$(ffprobe -v error -select_streams v:0 \
              -show_entries stream=width,height -of csv=p=0 "$out")
      [ "$got" = "${w},${h}" ] \
        || die "ffmpeg produced $got for a requested ${w}x${h} $fmt clip"
    fi
  }

  for fmt in yuv420p yuv422p yuv444p yuv440p nv12 nv21 gray \
             rgb24 bgr24 rgba bgra argb abgr rgb0 \
             yuv420p10le yuv422p10le yuv444p10le yuva420p \
             rgb48le rgba64le gbrp gbrap uyvy422 yuyv422 gray16le ya8; do
    gen_raw "$fmt" 64 64
  done
  for fmt in yuv420p yuv422p nv12 rgb24 yuv420p10le; do
    gen_raw "$fmt" 65 33
  done

  # Formats Video::formatNeedsDecoding() sends through swscale to RGBA rather
  # than to a GPU decoder: the CPU rescale path has no other consumer.
  for fmt in yuv410p bgr565le rgb555le pal8 bgr8; do
    gen_raw "$fmt" 64 64
  done
  gen_raw yuv410p 65 33

  # Colour-space inference in VideoDecoder::open_stream() branches on the frame
  # height when the stream tags none: <625 -> SMPTE170M, <720 -> BT470BG,
  # otherwise BT709. All three need a clip.
  gen_raw yuv420p 64 480
  gen_raw yuv420p 64 640
  gen_raw yuv420p 64 720

  # A file with an audio stream in front of the video one: open_stream() must
  # pick the video stream and mark the rest AVDISCARD_ALL.
  ffmpeg -nostdin -loglevel error -y \
    -f lavfi -i "sine=frequency=440:duration=1" \
    -f lavfi -i "testsrc=size=64x64:rate=25:duration=1" \
    -map 0:a -map 1:v -c:a aac -c:v libx264 -preset ultrafast -pix_fmt yuv420p \
    "$tmp/audio-video.mp4" \
    || die "ffmpeg could not produce the audio+video clip"

  # Malformed inputs: opening these must fail cleanly, not crash or hang.
  head -c 4096 "$tmp/h264.mp4" > "$tmp/truncated.mp4"
  head -c 2048 /dev/urandom    > "$tmp/garbage.bin"
  : > "$tmp/empty.bin"

  export SCORE_TEST_MEDIA_DIR="$tmp"
fi

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] \
   && [ -z "${QT_QPA_PLATFORM:-}" ]; then
  export QT_QPA_PLATFORM=offscreen
fi

"$@"
rc=$?
exit $rc
