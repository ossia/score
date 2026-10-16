#!/usr/bin/env bash
# Drives score_edit_fuzzer over every .score file of a directory, one process
# per (document, seed), like run-score-corpus.sh. A crash or a hang becomes a
# report line (CRASH_SIG<n> / TIMEOUT / EXIT_<n>) carrying the last edits the
# fuzzer logged, instead of killing the sweep.
#
# Usage: run-edit-fuzz.sh <scores-dir> <out-dir> [jobs] [seeds] [fuzzer]
#   seeds: space-separated list, default "1 2 3"
#   SCORE_EDIT_FUZZ_OPS (default 400), SCORE_EDIT_FUZZ_TIMEOUT (default 300 s)
#   SCORE_EDIT_FUZZ_SKIP: a previous results.jsonl; its (file, seed) pairs are
#   not run again (except CRASH_SIG15/TIMEOUT records, i.e. killed runs)
#
# Output: <out>/results.jsonl, <out>/verdicts.tsv, <out>/summary.txt, and one
# <out>/logs/<name>.<seed>.log per run (the fuzzer's stderr).
set -u

DIR="${1:?scores dir}"
OUT="${2:?out dir}"
JOBS="${3:-4}"
SEEDS="${4:-1 2 3}"
FUZZER="${5:-$(dirname "$0")/score_edit_fuzzer}"
OPS="${SCORE_EDIT_FUZZ_OPS:-400}"
TIMEOUT_PER_RUN="${SCORE_EDIT_FUZZ_TIMEOUT:-300}"

mkdir -p "$OUT/logs"
: > "$OUT/results.jsonl"
export FUZZER OUT OPS TIMEOUT_PER_RUN

run_one() {
  local f=$1 seed=$2
  local name
  name=$(echo "${f#"$DIR"/}" | tr '/' '_')
  local errf="$OUT/logs/$name.$seed.log"
  local out ec
  # SIGABRT on timeout: the fuzzer's crash handler then prints where it hung.
  out=$(timeout -s ABRT -k 10 "$TIMEOUT_PER_RUN" "$FUZZER" --seed "$seed" --ops "$OPS" "$f" 2>"$errf")
  ec=$?
  # Libraries print to stdout too (e.g. the SPZ loader): keep the verdict only.
  out=$(printf '%s\n' "$out" | grep '^{"mode":"edit-fuzz"' | tail -1)
  if [ -n "$out" ] && [ $ec -eq 0 ]; then
    echo "$out" >> "$OUT/results.jsonl"
    return
  fi
  local status
  if [ $ec -eq 124 ] || [ $ec -eq 137 ]; then status="TIMEOUT"
  elif [ $ec -gt 128 ]; then status="CRASH_SIG$((ec - 128))"
  else status="EXIT_$ec"; fi
  # Symbolize the fuzzer's "BT <module> <offset>" crash frames in place.
  if grep -q '^BT ' "$errf"; then
    grep '^BT ' "$errf" | head -40 | while read -r _ mod off; do
      [ "$mod" = "?" ] && { echo "  ?? $off"; continue; }
      "${LLVM_SYMBOLIZER:-llvm-symbolizer}" --obj="$mod" --functions=short --inlining=false --relativenames "$off" 2>/dev/null | head -2 | paste -sd' ' | sed 's/^/  /'
    done > "$errf.bt"
  fi
  local last
  last=$(grep '^STAGE' "$errf" | tail -3 | tr '\n' '|' | sed 's/\\/\\\\/g; s/"/\\"/g')
  local tailmsg
  tailmsg=$(grep -v '^STAGE' "$errf" | grep -i -E 'assert|abort|fatal|error|hardening|terminate|segmentation' | tail -4 | tr '\n' '|' | sed 's/\\/\\\\/g; s/"/\\"/g' | cut -c1-900)
  local bt=""
  [ -f "$errf.bt" ] && bt=$(grep -v -E 'crash_handler|__restore_rt|raise|abort|^  \?\?' "$errf.bt" | head -8 | sed 's/^ *//' | tr '\n' '|' | sed 's/\\/\\\\/g; s/"/\\"/g' | cut -c1-1500)
  printf '{"mode":"edit-fuzz","file":"%s","seed":%s,"status":"%s","last_stages":"%s","backtrace":"%s","note":"%s"}\n' \
    "$f" "$seed" "$status" "$last" "$bt" "$tailmsg" >> "$OUT/results.jsonl"
}
export -f run_one
export DIR

SKIP="${SCORE_EDIT_FUZZ_SKIP:-}"
find "$DIR" -name '*.score' | sort | while read -r f; do
  for s in $SEEDS; do printf '%s\t%s\n' "$f" "$s"; done
done | if [ -n "$SKIP" ]; then
  python3 -c '
import json, sys
done = set()
for l in open(sys.argv[1]):
    try: r = json.loads(l)
    except Exception: continue
    if r["status"] in ("CRASH_SIG15", "TIMEOUT"): continue
    done.add((r["file"], str(r["seed"])))
for l in sys.stdin:
    f, s = l.rstrip("\n").split("\t")
    if (f, s) not in done: print(l, end="")
' "$SKIP"
else cat; fi | xargs -r -P "$JOBS" -L1 bash -c 'run_one "$1" "$2"' _

# verdicts.tsv: file, seed, status
python3 - "$OUT" <<'EOF'
import json, sys, collections
out = sys.argv[1]
rows = [json.loads(l) for l in open(f"{out}/results.jsonl") if l.strip()]
with open(f"{out}/verdicts.tsv", "w") as v:
    for r in sorted(rows, key=lambda r: (r["file"], r["seed"])):
        v.write(f'{r["file"]}\t{r["seed"]}\t{r["status"]}\n')
c = collections.Counter(r["status"] for r in rows)
with open(f"{out}/summary.txt", "w") as s:
    s.write(f"{len(rows)} runs\n")
    for k, n in c.most_common():
        s.write(f"{n:5d} {k}\n")
print(open(f"{out}/summary.txt").read())
EOF
