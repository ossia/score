#!/usr/bin/env bash
# Sweep a directory of real .score documents through score_corpus_tester, one
# process per document, so a crash or a hang becomes a report LINE instead of a
# dead run. Same contract and the same verdict taxonomy as run-corpus.sh, which
# does this for media files (spec case P1-15).
#
#   run-score-corpus.sh [corpus-dir] [out-dir] [jobs] [tester-path]
#
# corpus-dir defaults to $SCORE_CORPUS_DIR, then to $HOME/ossia/score-corpus.
# When it does not exist the script exits 77 (the ctest SKIP code) and says so:
# the corpus is the user's own working files, it lives OUTSIDE the repository,
# and it must never be committed.
#
# Produces in <out-dir>:
#   results.jsonl   one verdict per document (tester JSON, or a synthesized
#                   CRASH_SIG<n>/TIMEOUT/EXIT_<n> record when the process died)
#   verdicts.tsv    "<relative path>\t<status>", sorted -- the shape that
#                   diffs against expected-verdicts.tsv
#   summary.txt     counts by status, plus every non-OK document
#
# Verdicts the tester itself emits: OK, UNKNOWN_UUID:<uuid>, LOADFAIL,
# ROUNDTRIP, GRAPHFAIL, BLANK. Verdicts this script synthesizes from the
# process's fate: CRASH_SIG<n>, TIMEOUT, EXIT_<n>.
#
# Gating: pass a baseline with --expected <file> (or EXPECTED_VERDICTS=<file>)
# and the script exits 1 when a document's verdict CHANGED from the baseline,
# the way shader_sweep_baseline_*.txt gates the shader sweeps. Without a
# baseline it only reports. The default baseline is
# $SCORE_CORPUS_DIR/expected-verdicts.tsv when that file exists.
#
# THE BASELINE IS DELIBERATELY NOT COMMITTED, and this is a deviation from
# SPEC-SCENE-RENDER-TESTS.md P1-15, which asks for a committed
# expected-verdicts.tsv. verdicts.tsv is one line per document keyed by its
# RELATIVE PATH, and those paths are the titles of the user's own working
# files. The spec's own rule for this case is "Never commit the user's
# documents"; committing 260 of their filenames to a public repository is the
# same disclosure in miniature. Generate the baseline next to the corpus
# instead:
#   run-score-corpus.sh && cp <out>/verdicts.tsv "$SCORE_CORPUS_DIR/expected-verdicts.tsv"
# and gate later runs against it. The aggregate counts are safe to quote and
# are recorded in LEDGER-SCENE-RENDER-TESTS.md.
set -uo pipefail

EXPECTED=""
args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --expected) EXPECTED="${2:?--expected needs a file}"; shift 2 ;;
    *) args+=("$1"); shift ;;
  esac
done
set -- "${args[@]+"${args[@]}"}"

CORPUS="${1:-${SCORE_CORPUS_DIR:-$HOME/ossia/score-corpus}}"
OUT="${2:-${SCORE_CORPUS_OUT:-/tmp/score-corpus-report}}"
JOBS="${3:-4}"
EXPECTED="${EXPECTED:-${EXPECTED_VERDICTS:-}}"
if [ -z "$EXPECTED" ] && [ -f "$CORPUS/expected-verdicts.tsv" ]; then
  EXPECTED="$CORPUS/expected-verdicts.tsv"
fi

find_tester() {
  if [ $# -ge 4 ] && [ -n "${4:-}" ]; then echo "$4"; return; fi
  if [ -n "${SCORE_CORPUS_TESTER:-}" ]; then echo "$SCORE_CORPUS_TESTER"; return; fi
  local here; here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  local c
  for c in \
      "$here/../../b-dyn/tests/corpus/score_corpus_tester" \
      "$here/../../build/tests/corpus/score_corpus_tester" \
      "$here/../../build-developer/tests/corpus/score_corpus_tester" \
      "$PWD/tests/corpus/score_corpus_tester" ; do
    [ -x "$c" ] && { echo "$c"; return; }
  done
  echo ""
}
TESTER="$(find_tester "$@")"

if [ ! -d "$CORPUS" ]; then
  echo "score corpus not found: $CORPUS" >&2
  echo "point SCORE_CORPUS_DIR at a directory of .score documents (it is never committed)" >&2
  exit 77
fi
if [ -z "$TESTER" ] || [ ! -x "$TESTER" ]; then
  echo "score_corpus_tester not found; build it or pass its path / set SCORE_CORPUS_TESTER" >&2
  exit 77
fi

mkdir -p "$OUT/shards" "$OUT/errs"
rm -f "$OUT"/shards/* "$OUT"/errs/* "$OUT/results.jsonl" "$OUT/summary.txt" "$OUT/verdicts.tsv"

# One document per process: an ASan report aborts (-> CRASH_SIG6 in the report)
# instead of exiting 1, and stderr is kept per document when non-empty.
export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1}"
export SCORE_AUDIO_BACKEND="${SCORE_AUDIO_BACKEND:-dummy}"
export SCORE_DISABLE_AUDIOPLUGINS="${SCORE_DISABLE_AUDIOPLUGINS:-1}"

SECONDS_PER_DOC="${SCORE_CORPUS_SECONDS:-6}"
TIMEOUT_PER_DOC="${SCORE_CORPUS_TIMEOUT:-120}"
EXTRA_ARGS="${SCORE_CORPUS_ARGS:-}"

export TESTER OUT CORPUS SECONDS_PER_DOC TIMEOUT_PER_DOC EXTRA_ARGS

run_one() {
  local f="$1"
  local key; key=$(printf '%s' "$f" | md5sum | cut -d' ' -f1)
  local shard="$OUT/shards/$key"
  local errf="$OUT/errs/$key"
  local out ec
  # shellcheck disable=SC2086
  out=$(timeout -k 10 "$TIMEOUT_PER_DOC" "$TESTER" --seconds "$SECONDS_PER_DOC" $EXTRA_ARGS "$f" 2>"$errf")
  ec=$?
  if [ -s "$errf" ]; then printf '== %s\n' "$f" >> "$errf"; else rm -f "$errf"; fi
  if [ $ec -eq 0 ] && [ -n "$out" ]; then
    printf '%s\n' "$out" >> "$shard"
  else
    local status
    if [ $ec -eq 124 ] || [ $ec -eq 137 ]; then status="TIMEOUT"
    elif [ $ec -gt 128 ]; then status="CRASH_SIG$((ec - 128))"
    else status="EXIT_$ec"; fi
    printf '{"mode":"scene","file":"%s","status":"%s","process_uuids":-1,"unknown_uuids":-1,"texture_outlets":-1,"windows":-1,"roundtrip":"","note":"process ended abnormally"}\n' \
      "$f" "$status" >> "$shard"
  fi
}
export -f run_one

find "$CORPUS" -type f \( -name '*.score' -o -name '*.scorebin' \) \
  | sort | xargs -r -P "$JOBS" -I{} bash -c 'run_one "$@"' _ {}

cat "$OUT"/shards/* > "$OUT/results.jsonl" 2>/dev/null || true
rm -rf "$OUT/shards"

# verdicts.tsv: relative path + status, sorted, one line per document. This is
# the artefact expected-verdicts.tsv is diffed against (see the header:
# that baseline lives next to the corpus and is never committed).
python3 - "$OUT/results.jsonl" "$CORPUS" > "$OUT/verdicts.tsv" <<'PYEOF'
import json, os, sys
rows = []
with open(sys.argv[1]) as fp:
    for line in fp:
        line = line.strip()
        if not line:
            continue
        try:
            r = json.loads(line)
        except Exception:
            continue
        rows.append((os.path.relpath(r["file"], sys.argv[2]), r["status"]))
for path, status in sorted(rows):
    print(f"{path}\t{status}")
PYEOF

{
  echo "== corpus: $CORPUS"
  echo "== documents: $(wc -l < "$OUT/verdicts.tsv")"
  echo
  echo "== status counts"
  cut -f2 "$OUT/verdicts.tsv" | sed 's/^UNKNOWN_UUID:.*/UNKNOWN_UUID/' \
    | sort | uniq -c | sort -rn
  echo
  echo "== non-OK verdicts"
  grep -vP '\tOK$' "$OUT/verdicts.tsv" | sort
  if ls "$OUT"/errs/* >/dev/null 2>&1; then
    echo
    echo "== documents with stderr output: $OUT/errs/"
    ls "$OUT/errs" | wc -l
  fi
} > "$OUT/summary.txt"

echo "report: $OUT/summary.txt"
echo "verdicts: $OUT/verdicts.tsv"
grep -cP '\tOK$' "$OUT/verdicts.tsv" | xargs echo "OK documents:"

rc=0
if [ -n "$EXPECTED" ]; then
  if [ ! -f "$EXPECTED" ]; then
    echo "expected-verdicts file not found: $EXPECTED" >&2
    exit 1
  fi
  # Only a REGRESSION gates: a document whose verdict is not what the baseline
  # recorded. A document missing from the baseline is reported, not fatal --
  # the corpus grows.
  python3 - "$EXPECTED" "$OUT/verdicts.tsv" <<'PYEOF'
import sys
def load(p):
    out = {}
    with open(p) as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            path, _, status = line.partition("\t")
            out[path] = status
    return out
base, cur = load(sys.argv[1]), load(sys.argv[2])
bad = new = 0
for path, status in sorted(cur.items()):
    if path not in base:
        print(f"NEW      {path}\t{status}")
        new += 1
    elif base[path] != status:
        print(f"CHANGED  {path}\t{base[path]} -> {status}")
        bad += 1
for path in sorted(set(base) - set(cur)):
    print(f"MISSING  {path}\t{base[path]}")
print(f"-- {bad} changed, {new} new, {len(set(base) - set(cur))} missing")
sys.exit(1 if bad else 0)
PYEOF
  rc=$?
fi
exit $rc
