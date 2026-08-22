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

want_video=0; want_media=0; want_matrix=0; want_gstreamer=0
while [ $# -gt 0 ]; do
  case "$1" in
    --video) want_video=1; want_gstreamer=1; shift;;
    --media) want_media=1; shift;;
    --matrix) want_matrix=1; shift;;
    --gstreamer) want_gstreamer=1; shift;;
    --) shift; break;;
    *) echo "with-virtual-media: unexpected arg '$1'" >&2; exit 2;;
  esac
done
[ $# -ge 1 ] || {
  echo "usage: $0 [--video] [--gstreamer] [--media] [--matrix] -- <harness> [args...]" >&2
  exit 2
}

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

command -v ffmpeg >/dev/null 2>&1 || die "ffmpeg not on PATH (this runner requires it)"

# GStreamer and a live PipeWire graph are only needed by the harnesses that ask
# for them. Requiring a running daemon for a harness that only decodes files
# would make a missing daemon look like a decoder failure.
if [ "$want_gstreamer" = 1 ]; then
  command -v gst-launch-1.0 >/dev/null 2>&1 \
    || die "gst-launch-1.0 not on PATH (--gstreamer/--video requires it)"
  gst-inspect-1.0 videotestsrc >/dev/null 2>&1 \
    || die "gstreamer has no videotestsrc element"
fi

if [ "$want_video" = 1 ]; then
  gst-inspect-1.0 pipewiresink >/dev/null 2>&1 \
    || die "gstreamer has no pipewiresink element"

  sock="${PIPEWIRE_RUNTIME_DIR:-${XDG_RUNTIME_DIR:-/run/user/$(id -u)}}/${PIPEWIRE_REMOTE:-pipewire-0}"
  [ -S "$sock" ] || die "no PipeWire socket at $sock"
  if command -v pw-cli >/dev/null 2>&1; then
    timeout 5 pw-cli info 0 >/dev/null 2>&1 \
      || die "PipeWire socket present but the core does not answer"
  fi
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

if [ "$want_matrix" = 1 ]; then
  # The container x codec matrix, plus the malformed and mid-stream-change
  # inputs, all derived from ONE master clip so a decoded frame can be compared
  # 1:1 against known pixels.
  #
  # The master is a grid of solid 40x40 blocks whose colour is a function of the
  # block index and the frame number. Solid blocks are what makes a per-pixel
  # assertion possible through a 4:2:0 codec at all: chroma subsampling and
  # deblocking only disturb the block borders, so the interior of a block still
  # carries the exact colour the pattern put there.
  #
  # Each frame ALSO rotates the grid by one column, on top of a per-frame colour
  # offset, so that two different frames are nowhere near each other: the
  # harness measures that separation and refuses to run if it is not far larger
  # than the tolerance it allows a codec.
  #
  # master.rgb is derived FROM master.nut rather than generated a second time,
  # so the ground truth the test reads and the bytes the encoders were fed
  # cannot drift apart.
  mdir="$tmp/matrix"
  mkdir -p "$mdir"
  MW=320; MH=240; MN=8; MBLK=40
  MCOLS=$((MW / MBLK))
  bx="mod(floor(X/${MBLK})+N,${MCOLS})"
  by="floor(Y/${MBLK})"
  geq="format=gbrp,geq=\
r='mod(${bx}*53+${by}*97+N*61,256)':\
g='mod(${bx}*29+${by}*181+N*137,256)':\
b='mod(${bx}*149+${by}*41+N*89,256)'"

  ffmpeg -nostdin -loglevel error -y -f lavfi -i "color=c=black:s=${MW}x${MH}:r=25:d=1" \
    -vf "$geq" -frames:v "$MN" -pix_fmt rgb24 -c:v rawvideo -f nut "$mdir/master.nut" \
    || die "ffmpeg could not produce the matrix master clip"
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master.nut" \
    -pix_fmt rgb24 -c:v rawvideo -f rawvideo "$mdir/master.rgb" \
    || die "ffmpeg could not flatten the matrix master to rgb24"
  want=$((MW * MH * 3 * MN))
  got=$(stat -c %s "$mdir/master.rgb" 2>/dev/null || stat -f %z "$mdir/master.rgb")
  [ "$got" = "$want" ] || die "matrix master is $got bytes, expected $want"

  # The same pattern at an ODD geometry, so that chroma rounding is exercised by
  # a picture and not only by a stride computation. 65x39 with 13-pixel blocks:
  # both dimensions odd, both divisible by the block size.
  OW=65; OH=39; OBLK=13
  obx="mod(floor(X/${OBLK})+N,$((OW / OBLK)))"
  oby="floor(Y/${OBLK})"
  ogeq="format=gbrp,geq=\
r='mod(${obx}*53+${oby}*97+N*61,256)':\
g='mod(${obx}*29+${oby}*181+N*137,256)':\
b='mod(${obx}*149+${oby}*41+N*89,256)'"
  # lavfi rounds an odd requested size DOWN to even, so the source is generated
  # at double size and scaled: without the explicit scale the odd axis silently
  # disappears and the whole point of this master with it.
  ffmpeg -nostdin -loglevel error -y \
    -f lavfi -i "color=c=black:s=$((OW * 2))x$((OH * 2)):r=25:d=1" \
    -vf "scale=${OW}:${OH},${ogeq}" -frames:v "$MN" \
    -pix_fmt rgb24 -c:v rawvideo -f nut "$mdir/master-odd.nut" \
    || die "ffmpeg could not produce the odd-geometry master"
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master-odd.nut" \
    -pix_fmt rgb24 -c:v rawvideo -f rawvideo "$mdir/master-odd.rgb" \
    || die "ffmpeg could not flatten the odd-geometry master"
  owant=$((OW * OH * 3 * MN))
  ogot=$(stat -c %s "$mdir/master-odd.rgb" 2>/dev/null || stat -f %z "$mdir/master-odd.rgb")
  [ "$ogot" = "$owant" ] \
    || die "odd master is $ogot bytes, expected $owant (geometry was rounded?)"

  # The same two masters in a container GSTREAMER can demux. This host's
  # gst-libav has no working NUT demuxer ("Unknown demuxer nut, no idea what to
  # do", 13 s to fail), so the .nut masters are for libav-side harnesses only.
  # Matroska + FFV1 in gbrp is lossless, so the picture that reaches an appsink
  # is still the master's exact pixels.
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master.nut" \
    -c:v ffv1 -pix_fmt gbrp "$mdir/master.mkv" \
    || die "ffmpeg could not produce the GStreamer-side master"
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master-odd.nut" \
    -c:v ffv1 -pix_fmt gbrp "$mdir/master-odd.mkv" \
    || die "ffmpeg could not produce the odd GStreamer-side master"

  # ...and as flat RGBA, for the GStreamer harness's `filesrc ! rawvideoparse`
  # source. RGBA rather than RGB24 because rawvideoparse gives a 24-bit row of
  # an odd-width frame a 4-byte-aligned stride (196 bytes for 65 pixels) while
  # the file has tightly packed 195-byte rows: the SOURCE would then be
  # misframed and every format would fail for a reason that has nothing to do
  # with score. Four bytes per pixel is aligned at any width.
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master.nut" \
    -pix_fmt rgba -c:v rawvideo -f rawvideo "$mdir/master.rgba" \
    || die "ffmpeg could not flatten the master to rgba"
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master-odd.nut" \
    -pix_fmt rgba -c:v rawvideo -f rawvideo "$mdir/master-odd.rgba" \
    || die "ffmpeg could not flatten the odd master to rgba"

  # codec-<container>-<codec>-<exact|yuvexact|lossy>.<ext>, discovered by name:
  # a row added here enters the sweep with no test change. The third field is
  # the fidelity the row is entitled to -- `exact` for an RGB lossless codec,
  # `yuvexact` for a lossless codec that still round-trips through a YUV
  # colour space, `lossy` for a quantiser. A codec this ffmpeg cannot produce is
  # skipped WITH ITS NAME RECORDED in unavailable.txt, so the test reports an
  # attributed gap instead of a smaller green matrix.
  : > "$mdir/unavailable.txt"
  enc() {
    local out="$1"; shift
    if ffmpeg -nostdin -loglevel error -y -i "$mdir/master.nut" "$@" "$mdir/$out" \
         2>"$tmp/enc.err"; then
      :
    else
      echo "$out: $(head -c 300 "$tmp/enc.err" | tr '\n' ' ')" >> "$mdir/unavailable.txt"
      rm -f "$mdir/$out"
    fi
  }

  enc codec-mp4-h264-lossy.mp4        -c:v libx264 -preset ultrafast -pix_fmt yuv420p
  enc codec-mp4-h265-lossy.mp4        -c:v libx265 -preset ultrafast -pix_fmt yuv420p -tag:v hvc1
  enc codec-mp4-av1-lossy.mp4         -c:v libsvtav1 -preset 12 -pix_fmt yuv420p
  enc codec-mp4-mjpeg-lossy.mp4       -c:v mjpeg -q:v 2 -pix_fmt yuvj420p
  enc codec-mkv-h264-lossy.mkv        -c:v libx264 -preset ultrafast -pix_fmt yuv420p
  enc codec-mkv-h265-lossy.mkv        -c:v libx265 -preset ultrafast -pix_fmt yuv420p
  enc codec-mkv-vp8-lossy.mkv         -c:v libvpx -deadline realtime -cpu-used 8 -b:v 4M -pix_fmt yuv420p
  enc codec-mkv-vp9-lossy.mkv         -c:v libvpx-vp9 -deadline realtime -cpu-used 8 -b:v 4M -pix_fmt yuv420p
  enc codec-mkv-mjpeg-lossy.mkv       -c:v mjpeg -q:v 2 -pix_fmt yuvj420p
  enc codec-mkv-ffv1-exact.mkv        -c:v ffv1 -pix_fmt gbrp
  enc codec-mkv-ffv1yuv-yuvexact.mkv  -c:v ffv1 -pix_fmt yuv444p
  enc codec-webm-vp8-lossy.webm       -c:v libvpx -deadline realtime -cpu-used 8 -b:v 4M -pix_fmt yuv420p
  enc codec-webm-vp9-lossy.webm       -c:v libvpx-vp9 -deadline realtime -cpu-used 8 -b:v 4M -pix_fmt yuv420p
  enc codec-mov-prores-lossy.mov      -c:v prores_ks -profile:v 3 -pix_fmt yuv422p10le
  enc codec-mov-h264-lossy.mov        -c:v libx264 -preset ultrafast -pix_fmt yuv420p
  enc codec-mov-rawuyvy-yuvexact.mov  -c:v rawvideo -pix_fmt uyvy422
  enc codec-mpegts-h264-lossy.ts      -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4
  enc codec-mpegts-h265-lossy.ts      -c:v libx265 -preset ultrafast -pix_fmt yuv420p -g 4
  enc codec-avi-mjpeg-lossy.avi       -c:v mjpeg -q:v 2 -pix_fmt yuvj420p
  enc codec-avi-utvideo-exact.avi     -c:v utvideo -pix_fmt gbrp
  enc codec-avi-rawvideo-exact.avi    -c:v rawvideo -pix_fmt bgr24
  enc codec-nut-rawvideo-exact.nut    -c:v rawvideo -pix_fmt rgb24
  enc codec-nut-rawyuv-yuvexact.nut   -c:v rawvideo -pix_fmt yuv444p

  # Malformed inputs, one per container family: opening these must fail cleanly
  # rather than crash, hang, or half-open.
  for f in mp4 mkv webm mov ts avi nut; do
    : > "$mdir/zero.$f"
  done
  for src in codec-mp4-h264-lossy.mp4 codec-mkv-h264-lossy.mkv \
             codec-mpegts-h264-lossy.ts codec-nut-rawvideo-exact.nut; do
    [ -f "$mdir/$src" ] || continue
    sz=$(stat -c %s "$mdir/$src" 2>/dev/null || stat -f %z "$mdir/$src")
    head -c $((sz * 2 / 5)) "$mdir/$src" > "$mdir/truncated-$src"
  done
  head -c 65536 /dev/urandom > "$mdir/garbage.mp4"

  # A container whose header claims a geometry the frames do not have: two
  # MPEG-TS segments of different sizes concatenated. The demuxer reports the
  # FIRST size; the frames after the join are the second one.
  ffmpeg -nostdin -loglevel error -y -f lavfi -i "testsrc2=size=320x240:rate=25" \
    -frames:v 12 -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4 \
    -f mpegts "$tmp/part-320.ts" || die "ffmpeg could not produce the 320x240 segment"
  ffmpeg -nostdin -loglevel error -y -f lavfi -i "testsrc2=size=160x120:rate=25" \
    -frames:v 12 -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4 \
    -f mpegts "$tmp/part-160.ts" || die "ffmpeg could not produce the 160x120 segment"
  cat "$tmp/part-320.ts" "$tmp/part-160.ts" > "$mdir/resolution-change.ts"

  # Streams that gain and lose an audio track mid-play, same technique.
  ffmpeg -nostdin -loglevel error -y \
    -f lavfi -i "testsrc2=size=160x120:rate=25" -f lavfi -i "sine=frequency=440" \
    -map 0:v -map 1:a -frames:v 12 -shortest \
    -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4 -c:a aac \
    -f mpegts "$tmp/part-av.ts" || die "ffmpeg could not produce the a+v segment"
  ffmpeg -nostdin -loglevel error -y -f lavfi -i "testsrc2=size=160x120:rate=25" \
    -frames:v 12 -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4 \
    -f mpegts "$tmp/part-v.ts" || die "ffmpeg could not produce the video-only segment"
  cat "$tmp/part-av.ts" "$tmp/part-v.ts" > "$mdir/track-loss.ts"
  cat "$tmp/part-v.ts" "$tmp/part-av.ts" > "$mdir/track-gain.ts"

  # Audio-only and video-only files of the same duration: the A/V clock check
  # below needs each half on its own as well as together.
  ffmpeg -nostdin -loglevel error -y -f lavfi -i "sine=frequency=440:duration=2" \
    -c:a aac "$mdir/audio-only.m4a" || die "ffmpeg could not produce the audio-only clip"
  ffmpeg -nostdin -loglevel error -y -f lavfi -i "testsrc2=size=160x120:rate=25" \
    -frames:v 50 -c:v libx264 -preset ultrafast -pix_fmt yuv420p \
    "$mdir/video-only.mp4" || die "ffmpeg could not produce the video-only clip"
  ffmpeg -nostdin -loglevel error -y \
    -f lavfi -i "testsrc2=size=160x120:rate=25" -f lavfi -i "sine=frequency=440" \
    -map 0:v -map 1:a -frames:v 50 -shortest \
    -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac \
    "$mdir/audio-video.mp4" || die "ffmpeg could not produce the a+v clip"

  # Network fixtures. The streaming harness spawns and kills the peers itself --
  # half of what it has to test is what happens when one GOES AWAY -- so all the
  # runner provides is the media they serve.
  #
  # stream-mpeg2.ts exists because a receiver joining an H.264 MPEG-TS mid-GOP
  # cannot recover its parameter sets on this ffmpeg, which would make the plain
  # UDP row a test of ffmpeg's resynchronisation rather than of score's demuxer.
  ffmpeg -nostdin -loglevel error -y -i "$mdir/master.nut" \
    -c:v mpeg2video -q:v 4 -g 4 -f mpegts "$mdir/stream-mpeg2.ts" \
    || die "ffmpeg could not produce the MPEG-2 transport stream"

  # The HLS playlist is the master looped into several segments: a receiver has
  # to survive losing whatever was already in flight when it connected, and a
  # single-segment playlist would make that indistinguishable from failure.
  mkdir -p "$mdir/hls"
  ffmpeg -nostdin -loglevel error -y -stream_loop 60 -i "$mdir/master.nut" \
    -c:v libx264 -preset ultrafast -pix_fmt yuv420p -g 4 \
    -f hls -hls_time 1 -hls_list_size 0 -hls_playlist_type vod \
    -hls_segment_filename "$mdir/hls/seg%03d.ts" "$mdir/hls/index.m3u8" \
    || die "ffmpeg could not produce the HLS playlist"

  export SCORE_TEST_MATRIX_DIR="$mdir"
  export SCORE_TEST_MATRIX_WIDTH="$MW"
  export SCORE_TEST_MATRIX_HEIGHT="$MH"
  export SCORE_TEST_MATRIX_FRAMES="$MN"
  export SCORE_TEST_MATRIX_BLOCK="$MBLK"
  export SCORE_TEST_MATRIX_ODD_BLOCK="$OBLK"
fi

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] \
   && [ -z "${QT_QPA_PLATFORM:-}" ]; then
  export QT_QPA_PLATFORM=offscreen
fi

"$@"
rc=$?
exit $rc
