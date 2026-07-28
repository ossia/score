#!/usr/bin/env bash
# GPU video decoder sweep (Gfx/Graph/decoders/*.hpp): generate one tiny
# 64x64 / 8-frame clip per pixel format the decoders handle, play each one
# through VideoDecoderTester (drop -> Video process -> window device ->
# DirectVideoNodeRenderer -> per-format GPUVideoDecoder) on llvmpipe, and
# record per-format pass/fail. Modeled on tests/integration/csf-sweep.sh.
#
#   tests/integration/video-decoder-sweep.sh [media-dir] [profile-dir]
#     (run from the build root; needs the SCORE_VIDEO_TESTER cmake option)
#
# rawvideo-in-NUT preserves the exact pix_fmt end-to-end; HAP variants use
# .mov (fourcc path), yuvj* use MJPEG-in-AVI. DXV has no ffmpeg encoder so it
# is not covered here (needs a Resolume-produced file).
#
# Exit codes per run: 0 = played, 4 = file not recognized as video process,
# anything else / timeout = failure.
# Set SWEEP_PROFILE=1 to write per-format LLVM coverage profiles.
set -u
TESTER="./VideoDecoderTester"
MEDIA="${1:-/tmp/testmedia}"
PROF="${2:-/tmp/testmedia/prof}"
DISP="${DISPLAY:-:0}"
PLAY_MS="${SCORE_VIDEO_TEST_MS:-2500}"

RAW_FMTS=(
  yuv420p yuv422p yuv444p yuv440p
  yuv420p10le yuv422p10le yuv444p10le
  yuv420p12le yuv422p12le yuv444p12le
  yuva420p yuva444p yuva444p10le yuva444p12le
  nv12 nv21 nv16 nv24 nv42
  p010le p016le p012le p210le p410le
  y210le vuya vuyx xv30le
  yuyv422 uyvy422
  rgb24 bgr24 rgba bgra rgb0 bgr0 argb abgr
  rgb48le bgr48le rgba64le bgra64le
  x2rgb10le x2bgr10le
  gray gray16le grayf32le ya8 ya16le
  gbrp gbrap gbrp10le gbrp12le gbrp16le
  gbrap10le gbrap12le gbrap16le gbrpf32le gbrapf32le
)

mkdir -p "$MEDIA" "$PROF"
FF="ffmpeg -hide_banner -loglevel error -y -f lavfi -i testsrc2=size=64x64:rate=25 -frames:v 8"

declare -A FILES
for f in "${RAW_FMTS[@]}"; do
  [ -f "$MEDIA/$f.nut" ] || $FF -pix_fmt "$f" -c:v rawvideo -strict -2 "$MEDIA/$f.nut" || continue
  FILES[$f]="$MEDIA/$f.nut"
done
for v in hap hap_alpha hap_q; do
  [ -f "$MEDIA/hap_$v.mov" ] || $FF -pix_fmt rgba -c:v hap -format "$v" "$MEDIA/hap_$v.mov" || continue
  FILES[hap_$v]="$MEDIA/hap_$v.mov"
done
for f in yuvj420p yuvj422p yuvj444p; do
  [ -f "$MEDIA/$f.avi" ] || $FF -pix_fmt "$f" -c:v mjpeg -q:v 2 -strict -2 "$MEDIA/$f.avi" || continue
  FILES[$f]="$MEDIA/$f.avi"
done

run() { # fmt file
  local fmt="$1" file="$2"
  local penv=()
  [ "${SWEEP_PROFILE:-0}" = 1 ] && penv=("LLVM_PROFILE_FILE=$PROF/$fmt-%m.profraw")
  rm -f ~/.config/ossia/failsafe.bit
  flock /tmp/score-harness.lock env \
    ASAN_OPTIONS=detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0 \
    DISPLAY="$DISP" SCORE_AUDIO_BACKEND=dummy \
    LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    SCORE_VIDEO_TEST_MS="$PLAY_MS" "${penv[@]}" \
    timeout 120 "$TESTER" "$file" >"$MEDIA/run-$fmt.log" 2>&1
}

echo "Sweeping ${#FILES[@]} pixel formats on llvmpipe..."
fail=0; ok=0; skip=0
for fmt in $(printf '%s\n' "${!FILES[@]}" | sort); do
  run "$fmt" "${FILES[$fmt]}"; rc=$?
  case $rc in
    0) s=OK; ok=$((ok+1));;
    4) s=skip; skip=$((skip+1));;
    *) s="FAIL($rc)"; fail=$((fail+1));;
  esac
  printf '%-14s %s\n' "$fmt" "$s"
done
echo "---"
echo "ok=$ok skipped=$skip failed=$fail (of ${#FILES[@]})"
[ "$fail" = 0 ]
