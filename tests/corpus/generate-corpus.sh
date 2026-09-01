#!/usr/bin/env bash
# Generate a matrix of test clips with the system ffmpeg: known synthetic
# content (testsrc2) pushed through every encoder/pixel-format/GOP-structure
# combination we care about. Complements the FATE suite: these files are
# well-formed by construction, so any tester failure on them is unambiguous.
set -uo pipefail

DEST="${1:-$HOME/ossia/video-corpus/generated}"
mkdir -p "$DEST"
FF=${FFMPEG:-ffmpeg}
SRC="testsrc2=duration=2:rate=25:size=320x240"
SRC_ODD="testsrc2=duration=2:rate=25:size=322x178"
LOG="$DEST/generate.log"
: > "$LOG"

have_encoder() { "$FF" -hide_banner -encoders 2>/dev/null | awk '{print $2}' | grep -qx "$1"; }

gen() { # gen <name> <encoder> <container-ext> [extra ffmpeg args...]
  local name="$1" enc="$2" ext="$3"; shift 3
  local out="$DEST/$name.$ext"
  [ -f "$out" ] && return 0
  if ! have_encoder "$enc"; then echo "SKIP (no encoder): $name" >> "$LOG"; return 0; fi
  if "$FF" -hide_banner -v error -y -f lavfi -i "$SRC" -c:v "$enc" "$@" -an "$out" 2>>"$LOG"; then
    echo "OK: $name.$ext" >> "$LOG"
  else
    echo "FAIL: $name.$ext" >> "$LOG"; rm -f "$out"
  fi
}

gen_odd() { # same, with a non-mod-16 frame size to exercise stride/crop paths
  local name="$1" enc="$2" ext="$3"; shift 3
  local out="$DEST/$name.$ext"
  [ -f "$out" ] && return 0
  if ! have_encoder "$enc"; then return 0; fi
  "$FF" -hide_banner -v error -y -f lavfi -i "$SRC_ODD" -c:v "$enc" "$@" -an "$out" 2>>"$LOG" \
    && echo "OK: $name.$ext" >> "$LOG" || { echo "FAIL: $name.$ext" >> "$LOG"; rm -f "$out"; }
}

# --- classic MPEG family: GOP structures, B-frames ---------------------------
gen mpeg1-ipb      mpeg1video mpg -g 12 -bf 2 -b:v 2M
gen mpeg2-ipb      mpeg2video mpg -g 12 -bf 2 -b:v 2M
gen mpeg2-mkv      mpeg2video mkv -g 12 -bf 2 -b:v 2M
gen mpeg4-ipb      mpeg4      mp4 -g 12 -bf 2
gen mpeg4-avi      mpeg4      avi -g 12 -bf 2
gen msmpeg4v3      msmpeg4    avi
gen h263p          h263p      avi
gen flv1           flv        flv

# --- H.264 / H.265: pixel formats, B-pyramids, open GOP ----------------------
gen h264-420       libx264    mp4 -g 50 -bf 3 -pix_fmt yuv420p -preset veryfast
gen h264-422       libx264    mp4 -pix_fmt yuv422p -preset veryfast
gen h264-444       libx264    mp4 -pix_fmt yuv444p -preset veryfast
gen h264-10bit     libx264    mp4 -pix_fmt yuv420p10le -preset veryfast
gen h264-mkv       libx264    mkv -g 25 -bf 2 -preset veryfast
gen h264-ts        libx264    ts  -g 25 -bf 2 -preset veryfast
gen h265-420       libx265    mp4 -x265-params log-level=none -preset veryfast
gen h265-10bit     libx265    mp4 -x265-params log-level=none -pix_fmt yuv420p10le -preset veryfast
gen_odd h264-odd   libx264    mp4 -preset veryfast

# --- open web codecs ---------------------------------------------------------
gen vp8            libvpx     webm -b:v 1M
gen vp9            libvpx-vp9 webm -b:v 1M -row-mt 1
gen av1            libaom-av1 mkv  -cpu-used 8 -b:v 1M
gen av1-svt        libsvtav1  mkv  -preset 12
gen theora         libtheora  ogv

# --- production / intermediate codecs ---------------------------------------
gen prores-std     prores_ks  mov -profile:v 2
gen prores-4444    prores_ks  mov -profile:v 4 -pix_fmt yuva444p10le
gen dnxhr          dnxhd      mov -profile:v dnxhr_hq -pix_fmt yuv422p
gen mjpeg          mjpeg      avi -q:v 3
gen ffv1           ffv1       mkv
gen ffv1-rgb       ffv1       mkv -pix_fmt bgr0
gen huffyuv        huffyuv    avi
gen utvideo        utvideo    avi
gen magicyuv       magicyuv   avi
gen cfhd           cfhd       mov
gen speedhq        speedhq    mkv
gen_odd prores-odd prores_ks  mov -profile:v 2

# --- raw & legacy ------------------------------------------------------------
gen rawvideo-420   rawvideo   nut -pix_fmt yuv420p
gen rawvideo-rgb   rawvideo   nut -pix_fmt rgb24
gen v210           v210       mov
gen qtrle          qtrle      mov
gen cinepak        cinepak    mov
gen msvideo1       msvideo1   avi
gen rpza           rpza       mov
gen snow           snow       avi
gen asv1           asv1       avi
gen asv2           asv2       avi
gen wmv1           wmv1       asf
gen wmv2           wmv2       asf
gen dv             dvvideo    dv  -s 720x576 -pix_fmt yuv420p -ar 48000
gen gif            gif        gif

# --- GPU-direct codecs (score forwards packets undecoded) --------------------
gen hap            hap        mov
gen hap-alpha      hap        mov -format hap_alpha
gen hap-q          hap        mov -format hap_q

echo "generated: $(ls "$DEST" | grep -vc -e '\.log$')" | tee -a "$LOG"
