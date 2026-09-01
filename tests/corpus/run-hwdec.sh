#!/usr/bin/env bash
# Validate score's hardware decoding against the software decode of the same
# files: every hw-decodable codec family from the FATE suite plus the
# generated matrix, through every hwaccel that actually engages on this
# machine.
#
#   run-hwdec.sh <fate-dir> <generated-dir> <out-dir> [jobs] [tester]
#
# HWDEC_ACCELS overrides the accel list (default: vaapi cuda qsv vulkan vdpau drm).
# Accels that never engage on a known-good H.264 probe are skipped up front.
set -uo pipefail

FATE="${1:?fate-suite dir}"
GEN="${2:?generated dir}"
OUT="${3:?output dir}"
JOBS="${4:-8}"
TESTER="${5:-$HOME/ossia/score/build-developer/bin/score_video_corpus_tester}"
ACCELS="${HWDEC_ACCELS:-vaapi cuda qsv vulkan vdpau drm}"

[ -x "$TESTER" ] || { echo "tester not found: $TESTER" >&2; exit 1; }
mkdir -p "$OUT/shards"
rm -f "$OUT"/shards/* "$OUT/results.jsonl" "$OUT/summary.txt"

# The hw-decodable corpus: conformance suites and codec families score's
# ffmpegCanDoHardwareDecoding() covers.
LIST="$OUT/files.txt"
: > "$LIST"
for d in h264 h264-444 h264-conformance h264-high-depth hevc hevc-conformance \
         vp8 vp8-test-vectors-r1 vp8_alpha vp9-test-vectors av1 av1-test-vectors \
         mpeg2 mpeg4 mjpeg mjpegb vc1 wmv8 prores; do
  [ -d "$FATE/$d" ] && find "$FATE/$d" -type f >> "$LIST"
done
ls "$GEN"/h264* "$GEN"/h265* "$GEN"/vp8* "$GEN"/vp9* "$GEN"/av1* \
   "$GEN"/mpeg1* "$GEN"/mpeg2* "$GEN"/mpeg4* "$GEN"/mjpeg* \
   "$GEN"/prores* "$GEN"/wmv* >> "$LIST" 2>/dev/null || true
sort -u "$LIST" -o "$LIST"
echo "files: $(wc -l < "$LIST")"

probe_file="$GEN/h264-420.mp4"
[ -f "$probe_file" ] || probe_file=$(grep -m1 h264-conformance "$LIST" || head -1 "$LIST")

ACTIVE_ACCELS=""
for a in $ACCELS; do
  if timeout 60 "$TESTER" --hwaccel "$a" "$probe_file" 2>/dev/null \
      | grep -qE '"status":"(OK|PIXEL|COUNT|PTS)'; then
    ACTIVE_ACCELS="$ACTIVE_ACCELS $a"
    echo "accel active:  $a"
  else
    echo "accel skipped: $a (does not engage on this machine)"
  fi
done
[ -n "$ACTIVE_ACCELS" ] || { echo "no hwaccel engages on this machine" >&2; exit 1; }

export TESTER OUT ACTIVE_ACCELS

run_one() {
  local f="$1"
  local shard="$OUT/shards/$(echo "$f" | md5sum | cut -d' ' -f1)"
  local a out ec status
  for a in $ACTIVE_ACCELS; do
    out=$(timeout -k 10 120 "$TESTER" --hwaccel "$a" "$f" 2>/dev/null)
    ec=$?
    if [ $ec -eq 0 ] && [ -n "$out" ]; then
      printf '%s\n' "$out" >> "$shard"
    else
      if [ $ec -eq 124 ] || [ $ec -eq 137 ]; then status="TIMEOUT"
      elif [ $ec -gt 128 ]; then status="CRASH_SIG$((ec - 128))"
      else status="EXIT_$ec"; fi
      printf '{"mode":"hwdec:%s","file":"%s","status":"%s","score_frames":-1,"ref_frames":-1,"ref_raw_frames":-1,"first_mismatch":-1,"native_format":"","requested":"%s","engaged":"","out_format":"","note":"process ended abnormally"}\n' \
        "$a" "$f" "$status" "$a" >> "$shard"
    fi
  done
}
export -f run_one

xargs -r -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {} < "$LIST"

cat "$OUT"/shards/* > "$OUT/results.jsonl" 2>/dev/null || true
rm -rf "$OUT/shards"

{
  echo "== hwdec corpus: $(wc -l < "$LIST") files, accels:$ACTIVE_ACCELS"
  echo
  echo "== status counts by accel"
  grep -o '"mode":"[^"]*","file":"[^"]*","status":"[^"]*"' "$OUT/results.jsonl" \
    | sed 's/.*"mode":"hwdec:\([^"]*\)".*"status":"\([^"]*\)"/\1 \2/' \
    | sort | uniq -c | sort -rn
  echo
  echo "== engaged decoder histogram"
  grep -o '"requested":"[^"]*","engaged":"[^"]*"' "$OUT/results.jsonl" \
    | sort | uniq -c | sort -rn
  echo
  echo "== hard failures (crash, timeout, count/pts, non-minor pixel)"
  grep -vE '"status":"(OK|SKIP|HW_FALLBACK|NOT_APPLICABLE|PIXEL_MISMATCH_MINOR|PIXEL_DRIFT|PIXEL_MISMATCH_DAMAGED)"' "$OUT/results.jsonl" | sort
  echo
  echo "== drift / minor / damaged (for review)"
  grep -E '"status":"(PIXEL_MISMATCH_MINOR|PIXEL_DRIFT|PIXEL_MISMATCH_DAMAGED)"' "$OUT/results.jsonl" | sort
} > "$OUT/summary.txt"

echo "report: $OUT/summary.txt"
grep -c '"status":"OK"' "$OUT/results.jsonl" | xargs echo "OK lines:"
