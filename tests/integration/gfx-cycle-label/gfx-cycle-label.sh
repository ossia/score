#!/usr/bin/env bash
# Two ISF processes cabled into each other with immediate cables form a cycle
# the execution graph refuses. The error libossia logs must name the two
# processes, not the generic "Gfx::filter_node" of every Gfx node.
set -uo pipefail

SCORE="${OSSIA_SCORE:-}"
[ -x "$SCORE" ] || { echo "SKIP: OSSIA_SCORE not set"; exit 77; }
HERE="$(cd "$(dirname "$0")" && pwd)"
FS="$HERE/../../gfx/corpus/isf-passthrough-plain.fs"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/gfx-cycle-label.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT

cat > "$OUT/cycle.js" <<JS
var itv = Score.rootInterval();
var a = Score.createProcess(itv, "ISF Shader", "$FS");
var b = Score.createProcess(itv, "ISF Shader", "$FS");
Score.setName(a, "cycle-label-alpha");
Score.setName(b, "cycle-label-beta");
if(!Score.createCable(Score.outlet(a, 0), Score.inlet(b, 0))) console.log("CABLE-FAILED");
if(!Score.createCable(Score.outlet(b, 0), Score.inlet(a, 0))) console.log("CABLE-FAILED");
Score.play();
console.log("CYCLE-PLAYING");
JS

env SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 SCORE_DISABLE_LV2=1 \
    SCORE_DISABLE_FAILSAFE=1 SCORE_DISABLE_LIBRARY=1 QT_FORCE_STDERR_LOGGING=1 \
    "$SCORE" --no-restore --script "$OUT/cycle.js" > "$OUT/log" 2>&1 &
PID=$!
for _ in $(seq 1 90); do
  grep -q "not a DAG" "$OUT/log" && break
  grep -qE "CABLE-FAILED|TypeError|ReferenceError" "$OUT/log" && break
  kill -0 "$PID" 2>/dev/null || break
  sleep 1
done
kill -KILL "$PID" 2>/dev/null
wait "$PID" 2>/dev/null

line="$(grep "not a DAG" "$OUT/log" | head -1)"
echo "${line:-no cycle error logged}"
if grep -q "CABLE-FAILED" "$OUT/log"; then echo "FAIL: cable refused"; exit 1; fi
[[ "$line" == *cycle-label-alpha* && "$line" == *cycle-label-beta* ]] || { echo "FAIL"; tail -20 "$OUT/log"; exit 1; }
echo "PASS"
