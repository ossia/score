#!/usr/bin/env bash
# VIDEO DECODE-CORRECTNESS matrix.
#
# Proves each SOFTWARE-decodable pixel format in
# src/plugins/score-plugin-gfx/Gfx/Graph/decoders/ decodes to the RIGHT
# pixels, not merely "renders non-blank":
#   1. ffmpeg encodes a KNOWN static pattern (smpte bars + gradient + a
#      solid R/G/B/gray index band) into a container that PRESERVES the
#      target pix_fmt (rawvideo-in-NUT; MJPEG-in-AVI for yuvj*; HAP-in-MOV),
#      verified with ffprobe (many muxers silently convert).
#   2. ffmpeg also decodes that clip back to RGBA -> the per-format
#      reference image (bakes in the format's own loss: subsampling,
#      DXT quantization, ... so the comparison isolates score's decoder).
#   3. VideoDecoderTester --expect <ref> --psnr <thresh> plays the clip
#      through the real Gfx graph (drop -> Video process -> forced-offscreen
#      window device -> GPUVideoDecoder -> readback) and asserts the
#      rendered RGBA matches the reference within a per-format PSNR bound.
#   4. Fuzz: truncated / garbage / empty clips must be rejected gracefully
#      (no crash, no hang).
#
# YUV colorspace: clips are encoded untagged with an explicit bt601 swscale
# matrix; score's colorMatrix() fallback for untagged <1280px content is
# BT.601 limited (ColorSpace.hpp), and the references are decoded with the
# same explicit matrix, so both sides agree. yuvj* goes through MJPEG which
# tags AVCOL_RANGE_JPEG -> full-range BT.601 on both sides.
#
# Environment: needs an X DISPLAY (llvmpipe over GLX — the offscreen QPA of
# Qt 6.4 can create neither GL nor Vulkan contexts headless). Exit 77 skips
# when ffmpeg / the tester / a display is unavailable.
#
# Usage:
#   tests/integration/video-decode-correctness.sh [--gen-only] [--formats "a b c"]
#   VIDEO_TESTER=<path>  MEDIA=<dir>  SCORE_VIDEO_TEST_MS=<ms>
#
# Serialization: each tester run takes flock /tmp/score-harness.lock itself
# (do NOT hold it around the whole script — see EXHAUSTIVE-TEST-PLAN note).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../.." && pwd)"
TESTER="${VIDEO_TESTER:-$SRCROOT/build-sanitizers/VideoDecoderTester}"
[ -x "$TESTER" ] || TESTER="./VideoDecoderTester"
MEDIA="${MEDIA:-${TMPDIR:-/tmp}/score-video-decode-$(id -u)}"
DISP="${DISPLAY:-:0}"
PLAY_MS="${SCORE_VIDEO_TEST_MS:-150000}"
RUN_TIMEOUT="${RUN_TIMEOUT:-240}"
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"
W=128 H=128 FRAMES=12

GEN_ONLY=0
FORMATS_OVERRIDE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --gen-only) GEN_ONLY=1; shift ;;
    --formats) FORMATS_OVERRIDE="$2"; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

# ---- prerequisites -> ctest SKIP (77) ---------------------------------------
command -v ffmpeg  >/dev/null || { echo "SKIP: ffmpeg not found";  exit 77; }
command -v ffprobe >/dev/null || { echo "SKIP: ffprobe not found"; exit 77; }
[ -x "$TESTER" ] || { echo "SKIP: VideoDecoderTester not built ($TESTER)"; exit 77; }
if [ "$GEN_ONLY" = 0 ]; then
  [ -n "${DISPLAY:-$DISP}" ] || { echo "SKIP: no DISPLAY"; exit 77; }
  if command -v xset >/dev/null && ! DISPLAY="$DISP" xset q >/dev/null 2>&1; then
    echo "SKIP: X server on $DISP not reachable"; exit 77
  fi
fi

mkdir -p "$MEDIA"
FF="ffmpeg -nostdin -hide_banner -loglevel error -y"

# ---- format matrix ----------------------------------------------------------
# name  encoder-class  psnr-threshold
#   raw     : rawvideo in NUT, RGB/gray — no colorspace conversion
#   yuv     : rawvideo in NUT, forced BT.601 limited both ways
#   yuvfull : MJPEG in AVI (yuvj*, full-range BT.601)
#   hap     : HAP in MOV (DXT-compressed; ffmpeg-decoded reference)
# Thresholds calibrated on llvmpipe 2026-07; each is >=6dB
# below the measured value but high enough that a plane swap / wrong matrix /
# channel-order bug (all measured <20dB) fails.
MATRIX="
rgb24        raw      45
bgr24        raw      45
rgba         raw      45
bgra         raw      45
rgb0         raw      45
bgr0         raw      45
argb         raw      45
abgr         raw      45
rgb48le      raw      45
bgr48le      raw      45
rgba64le     raw      45
bgra64le     raw      45
x2rgb10le    raw      45
x2bgr10le    raw      45
gray         raw      45
gray16le     raw      45
gbrp         raw      45
gbrap        raw      45
gbrp10le     raw      45
gbrp12le     raw      45
gbrp16le     raw      45
gbrap10le    raw      45
gbrap12le    raw      45
gbrap16le    raw      45
gbrpf32le    raw      45
gbrapf32le   raw      45
# 4:2:0 / 4:2:2 chroma-subsampled -> looser 26 dB: on the pattern's hard
# color edges the GPU shader's bilinear chroma upsample differs from
# ffmpeg's swscale upsampler (the reference decoder), clustering these at
# ~28.5-29.7 dB. Full-chroma 4:4:4 stays at 30 (measures 46-65 dB) and 4:4:0
# at 30 (37 dB) — a plane-swap / wrong-matrix bug drops to <20 dB, still caught.
yuv420p      yuv      26
yuv422p      yuv      26
yuv444p      yuv      30
yuv440p      yuv      30
yuv420p10le  yuv      26
yuv422p10le  yuv      26
yuv444p10le  yuv      30
yuv420p12le  yuv      26
yuv422p12le  yuv      26
yuv444p12le  yuv      30
yuva420p     yuv      26
yuva444p     yuv      30
yuva444p10le yuv      30
yuva444p12le yuv      30
yuyv422      yuv      26
uyvy422      yuv      26
yuvj420p     yuvfull  28
yuvj422p     yuvfull  28
yuvj444p     yuvfull  28
hap          hap      30
hap_alpha    hap      30
hap_q        hap      25
"

# Known-bug formats: decode to the WRONG pixels. Empty now — the two decoder
# bugs this matrix originally found are fixed (verified under ASAN: rgb24/bgr24
# 15 dB -> 99 dB, rgba64le/bgra64le black -> 51 dB), so every format is asserted
# as a hard PASS. Re-add a format here only if a future decoder bug needs to be
# tracked without failing the suite.
#   FIXED: rgb24/bgr24 — RGB24Decoder R8 data texture no longer flagged
#          QRhiTexture::sRGB (the sampler was EOTF-linearizing raw bytes).
#   FIXED: rgba64le/bgra64le — now decoded by RGBA64Decoder (R16 x w*4 packed +
#          texelFetch reassembly) instead of reinterpreting UNORM16 data as
#          half-float in an RGBA16F texture (which rendered pure black).
declare -A XFAIL

# ---- media generation -------------------------------------------------------
gen_pattern() {
  [ -f "$MEDIA/pattern.png" ] && return 0
  $FF \
    -f lavfi -i "smptebars=size=${W}x$((H/2)):rate=1" \
    -f lavfi -i "gradients=s=${W}x$((H/4)):c0=black:c1=white:x0=0:y0=$((H/8)):x1=$((W-1)):y1=$((H/8)):r=1:n=2" \
    -f lavfi -i "color=c=red:s=$((W/4))x$((H/4)):r=1" \
    -f lavfi -i "color=c=lime:s=$((W/4))x$((H/4)):r=1" \
    -f lavfi -i "color=c=blue:s=$((W/4))x$((H/4)):r=1" \
    -f lavfi -i "color=c=gray:s=$((W/4))x$((H/4)):r=1" \
    -filter_complex "[2:v][3:v][4:v][5:v]hstack=4[band];[0:v][1:v][band]vstack=3,format=rgb24" \
    -frames:v 1 "$MEDIA/pattern.png"
}

probe_fmt() { # file -> stored pix_fmt
  ffprobe -v error -select_streams v:0 -show_entries stream=pix_fmt \
    -of default=nw=1:nk=1 "$1" 2>/dev/null | head -1
}

# gen_one <fmt> <class>: makes $MEDIA/<fmt>.<ext> + $MEDIA/ref-<fmt>.png.
# Returns 1 (NOT-PRODUCED) if the encoder can't emit the format or the muxer
# silently converted it.
gen_one() {
  local fmt="$1" class="$2" clip probe expected_probe="$fmt"
  local in=(-loop 1 -i "$MEDIA/pattern.png" -frames:v $FRAMES -r 25)
  case "$class" in
    raw)
      clip="$MEDIA/$fmt.nut"
      [ -f "$clip" ] || $FF "${in[@]}" -vf "format=$fmt" \
          -c:v rawvideo -strict -2 "$clip" 2>/dev/null || return 1
      [ -f "$MEDIA/ref-$fmt.png" ] || $FF -i "$clip" \
          -vf "format=rgba" -frames:v 1 "$MEDIA/ref-$fmt.png" || return 1
      ;;
    yuv)
      clip="$MEDIA/$fmt.nut"
      [ -f "$clip" ] || $FF "${in[@]}" \
          -vf "scale=out_color_matrix=bt601:out_range=tv,format=$fmt" \
          -c:v rawvideo -strict -2 "$clip" 2>/dev/null || return 1
      [ -f "$MEDIA/ref-$fmt.png" ] || $FF -i "$clip" \
          -vf "scale=in_color_matrix=bt601:in_range=tv,format=rgba" \
          -frames:v 1 "$MEDIA/ref-$fmt.png" || return 1
      ;;
    yuvfull)
      clip="$MEDIA/$fmt.avi"
      [ -f "$clip" ] || $FF "${in[@]}" \
          -vf "scale=out_color_matrix=bt601:out_range=jpeg,format=$fmt" \
          -c:v mjpeg -q:v 1 -strict -2 "$clip" 2>/dev/null || return 1
      [ -f "$MEDIA/ref-$fmt.png" ] || $FF -i "$clip" \
          -vf "scale=in_color_matrix=bt601:in_range=jpeg,format=rgba" \
          -frames:v 1 "$MEDIA/ref-$fmt.png" || return 1
      ;;
    hap)
      local variant="${fmt#hap}"; variant="${variant#_}"; [ -z "$variant" ] && variant=hap
      case "$fmt" in hap) variant=hap;; hap_alpha) variant=hap_alpha;; hap_q) variant=hap_q;; esac
      clip="$MEDIA/$fmt.mov"
      [ -f "$clip" ] || $FF "${in[@]}" -vf "format=rgba" \
          -c:v hap -format "$variant" "$clip" 2>/dev/null || return 1
      # HAP stores DXT: the reference must include the DXT loss
      [ -f "$MEDIA/ref-$fmt.png" ] || $FF -i "$clip" \
          -vf "format=rgba" -frames:v 1 "$MEDIA/ref-$fmt.png" || return 1
      expected_probe="" # fourcc codec: pix_fmt probe is rgba/rgb0, not meaningful
      ;;
  esac
  if [ -n "$expected_probe" ]; then
    probe="$(probe_fmt "$clip")"
    if [ "$probe" != "$expected_probe" ]; then
      echo "  (muxer converted $fmt -> $probe)" >&2
      rm -f "$clip" "$MEDIA/ref-$fmt.png"
      return 1
    fi
  fi
  echo "$clip"
  return 0
}

gen_fuzz() {
  # truncated: a valid rawvideo NUT cut mid-frame
  if [ ! -f "$MEDIA/fuzz-truncated.nut" ] && [ -f "$MEDIA/rgba.nut" ]; then
    local sz; sz=$(stat -c %s "$MEDIA/rgba.nut")
    head -c $((sz * 2 / 5)) "$MEDIA/rgba.nut" > "$MEDIA/fuzz-truncated.nut"
  fi
  # garbage bytes with a video-ish extension
  [ -f "$MEDIA/fuzz-garbage.mov" ] \
    || head -c 65536 /dev/urandom > "$MEDIA/fuzz-garbage.mov"
  # empty file
  : > "$MEDIA/fuzz-empty.nut"
}

# ---- one tester run ---------------------------------------------------------
run_tester() { # clip ref thresh logfile -> tester rc
  # PLAY_MS_OVERRIDE shortens the in-app deadline (used by the fuzz runs,
  # which are expected to never render anything).
  local clip="$1" ref="$2" thresh="$3" log="$4"
  rm -f "$HOME/.config/ossia/failsafe.bit"
  local expect_args=()
  [ -n "$ref" ] && expect_args=(--expect "$ref" --psnr "$thresh")
  flock /tmp/score-harness.lock env \
    DISPLAY="$DISP" QT_QPA_PLATFORM=xcb \
    SCORE_AUDIO_BACKEND=dummy \
    LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    ASAN_OPTIONS="$ASAN" \
    SCORE_VIDEO_TEST_MS="${PLAY_MS_OVERRIDE:-$PLAY_MS}" \
    timeout "$RUN_TIMEOUT" "$TESTER" "$clip" "${expect_args[@]}" \
      >"$log" 2>&1
}

# ---- main -------------------------------------------------------------------
gen_pattern || { echo "FAIL: cannot generate pattern.png"; exit 1; }

declare -A CLIP THRESH
ORDER=()
NOT_PRODUCED=()
while read -r fmt class thresh; do
  [ -z "$fmt" ] && continue
  case "$fmt" in \#*) continue ;; esac   # skip comment lines in $MATRIX
  if [ -n "$FORMATS_OVERRIDE" ]; then
    case " $FORMATS_OVERRIDE " in *" $fmt "*) ;; *) continue ;; esac
  fi
  if clip=$(gen_one "$fmt" "$class"); then
    CLIP[$fmt]="$clip"; THRESH[$fmt]="$thresh"; ORDER+=("$fmt")
  else
    NOT_PRODUCED+=("$fmt")
  fi
done <<< "$MATRIX"
gen_fuzz

echo "media: ${#ORDER[@]} clips in $MEDIA (${#NOT_PRODUCED[@]} not producible: ${NOT_PRODUCED[*]:-none})"
[ "$GEN_ONLY" = 1 ] && exit 0

pass=0; fail=0; skip=0; xfail=0
for fmt in "${ORDER[@]}"; do
  printf '%-14s' "$fmt"
  run_tester "${CLIP[$fmt]}" "$MEDIA/ref-$fmt.png" "${THRESH[$fmt]}" "$MEDIA/run-$fmt.log"
  rc=$?
  psnr=$(grep -a "DECODE-PSNR" "$MEDIA/run-$fmt.log" | tail -1 | awk '{print $2}')
  known="${XFAIL[$fmt]:-}"
  if [ -n "$known" ]; then
    # Documented decoder bug: must still decode (rc 5, wrong pixels) or pass,
    # and must NOT crash. A crash or a sudden PASS both deserve attention.
    case $rc in
      5) echo "XFAIL (psnr=${psnr:--} < ${THRESH[$fmt]}; known bug: $known)"; xfail=$((xfail+1));;
      0) echo "XPASS (psnr=${psnr:-?} — decoder now correct? drop XFAIL[$fmt])"; xfail=$((xfail+1));;
      *) echo "FAIL  (rc=$rc — expected wrong-pixels, got crash/hang, see $MEDIA/run-$fmt.log)"; fail=$((fail+1));;
    esac
    grep -aq "ERROR: AddressSanitizer" "$MEDIA/run-$fmt.log" \
      && { echo "               ^ ASAN error under known-bug clip"; fail=$((fail+1)); }
    continue
  fi
  case $rc in
    0) echo "PASS  (psnr=${psnr:-?} >= ${THRESH[$fmt]})"; pass=$((pass+1));;
    4) echo "SKIP  (not recognized as video)"; skip=$((skip+1));;
    5) echo "FAIL  (psnr=${psnr:--} < ${THRESH[$fmt]}, see $MEDIA/run-$fmt.log)"; fail=$((fail+1));;
    124) echo "FAIL  (timeout)"; fail=$((fail+1));;
    *) echo "FAIL  (rc=$rc — crash?, see $MEDIA/run-$fmt.log)"; fail=$((fail+1));;
  esac
done

# ---- fuzz: graceful rejection, ASAN-clean, no hang --------------------------
for fz in fuzz-truncated.nut fuzz-garbage.mov fuzz-empty.nut; do
  [ -f "$MEDIA/$fz" ] || continue
  printf '%-14s' "$fz"
  PLAY_MS_OVERRIDE=20000 run_tester "$MEDIA/$fz" "$MEDIA/ref-rgba.png" 30 "$MEDIA/run-$fz.log"
  rc=$?
  # Acceptable: 0 (partial clip still decodes), 4 (not recognized),
  # 5 (never matched the reference). Anything else is a crash or a hang.
  case $rc in
    0|4|5)
      if grep -aq "ERROR: AddressSanitizer" "$MEDIA/run-$fz.log"; then
        echo "FAIL  (ASAN error, see $MEDIA/run-$fz.log)"; fail=$((fail+1))
      else
        echo "PASS  (graceful, rc=$rc)"; pass=$((pass+1))
      fi;;
    124) echo "FAIL  (hang/timeout)"; fail=$((fail+1));;
    *) echo "FAIL  (rc=$rc — crash, see $MEDIA/run-$fz.log)"; fail=$((fail+1));;
  esac
done

echo "----"
echo "video-decode-correctness: $pass pass, $fail fail, $xfail xfail(known-bug), $skip skip, ${#NOT_PRODUCED[@]} not-produced"
[ "$fail" = 0 ]
