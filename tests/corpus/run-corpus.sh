#!/usr/bin/env bash
# Sweep a directory of media files through score_video_corpus_tester, one
# process per file per mode, so crashes and hangs become report lines.
#
#   run-corpus.sh <corpus-dir> <out-dir> [jobs] [tester-path]
#
# Produces in <out-dir>:
#   results.jsonl   one verdict per file+mode (tester JSON, or a synthesized
#                   CRASH/TIMEOUT record when the process died)
#   summary.txt     counts by mode+status and the list of non-OK files
set -uo pipefail

CORPUS="${1:?corpus dir}"
OUT="${2:?output dir}"
JOBS="${3:-8}"
TESTER="${4:-$HOME/ossia/score/build-developer/bin/score_video_corpus_tester}"

[ -x "$TESTER" ] || { echo "tester not found: $TESTER" >&2; exit 1; }
mkdir -p "$OUT/shards" "$OUT/errs"
rm -f "$OUT"/shards/* "$OUT"/errs/* "$OUT/results.jsonl" "$OUT/summary.txt"

# Sanitizer-friendly defaults: an ASan report aborts (-> CRASH_SIG6 in the
# report) instead of exit(1), and stderr is kept per file when non-empty.
export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}"

export TESTER OUT

run_one() {
  local f="$1"
  local key; key=$(echo "$f" | md5sum | cut -d' ' -f1)
  local shard="$OUT/shards/$key"
  local mode flag tmo out ec
  for mode in direct playback seek; do
    case $mode in
      direct)   flag="";              tmo=200 ;;
      playback) flag="--playback";    tmo=160 ;;
      seek)     flag="--seek-stress"; tmo=160 ;;
    esac
    local errf="$OUT/errs/$key.$mode"
    # shellcheck disable=SC2086
    out=$(timeout -k 10 $tmo "$TESTER" $flag "$f" 2>"$errf")
    ec=$?
    if [ -s "$errf" ]; then printf '== %s\n' "$f" >> "$errf"; else rm -f "$errf"; fi
    if [ $ec -eq 0 ] && [ -n "$out" ]; then
      printf '%s\n' "$out" >> "$shard"
    else
      local status
      if [ $ec -eq 124 ] || [ $ec -eq 137 ]; then status="TIMEOUT"
      elif [ $ec -gt 128 ]; then status="CRASH_SIG$((ec - 128))"
      else status="EXIT_$ec"; fi
      printf '{"mode":"%s","file":"%s","status":"%s","score_frames":-1,"ref_frames":-1,"ref_raw_frames":-1,"first_mismatch":-1,"native_format":"","note":"process ended abnormally"}\n' \
        "$mode" "$f" "$status" >> "$shard"
      # A file whose direct decode already crashed will almost surely crash
      # the other modes too; still try them — different code paths.
    fi
  done
}
export -f run_one

find "$CORPUS" -type f \
    ! -name '*.log' ! -name '*.txt' ! -name '*.md' ! -name '*.py' \
    ! -name '*.sh' ! -name '*.xml' ! -name '*.cue' \
  | sort | xargs -r -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {}

cat "$OUT"/shards/* > "$OUT/results.jsonl" 2>/dev/null || true
rm -rf "$OUT/shards"

{
  echo "== corpus: $CORPUS"
  echo "== files x modes: $(wc -l < "$OUT/results.jsonl")"
  echo
  echo "== status counts by mode"
  grep -o '"mode":"[^"]*","file":"[^"]*","status":"[^"]*"' "$OUT/results.jsonl" \
    | sed 's/.*"mode":"\([^"]*\)".*"status":"\([^"]*\)"/\1 \2/' \
    | sort | uniq -c | sort -rn
  echo
  echo "== non-OK, non-SKIP verdicts"
  grep -vE '"status":"(OK|SKIP)"' "$OUT/results.jsonl" | sort
  if ls "$OUT"/errs/* >/dev/null 2>&1; then
    echo
    echo "== files with stderr output (sanitizer reports, libav errors): $OUT/errs/"
    ls "$OUT/errs" | wc -l
  fi
} > "$OUT/summary.txt"

echo "report: $OUT/summary.txt"
grep -c '"status":"OK"' "$OUT/results.jsonl" | xargs echo "OK lines:"
