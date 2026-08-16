#!/usr/bin/env bash
# Acceptance test for deterministic frame stepping.
#
# Renders the SAME tester to the SAME frame index in two separate processes and
# requires the two PNGs to be byte-identical. Two processes rather than two
# grabs in one, because anything cached in the process would hide exactly the
# nondeterminism this is looking for.
#
# If this fails, golden references cannot be pinned: frame N is not a stable
# picture, and every stored reference is a snapshot of one particular run.
#
#   frame-determinism.sh [--score PATH] [--case NAME] [--frame N]
#
# Exit 0 iff the two renders agree.
set -uo pipefail

SCORE="${OSSIA_SCORE:-}"
CASE="build-isf-solid-color"
FRAME=30
OUT="${TMPDIR:-/tmp}/score-frame-determinism.$$"

while [ $# -gt 0 ]; do
  case "$1" in
    --score) SCORE="$2"; shift 2 ;;
    --case)  CASE="$2";  shift 2 ;;
    --frame) FRAME="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$SCORE" ] || { echo "FATAL: set OSSIA_SCORE or pass --score"; exit 2; }
[ -x "$SCORE" ] || { echo "FATAL: not executable: $SCORE"; exit 2; }

mkdir -p "$OUT"
trap 'rm -rf "$OUT"' EXIT

# The tester scripts resolve their own paths through <LIBRARY>:, so the only
# thing this needs to know is the case name.
render() {
  local n="$1"
  cat > "$OUT/run$n.js" <<JS
eval(Score.readFile("<LIBRARY>:/packages/csf-examples/csf-testers/tests-scene/scripts/${CASE}.js"));
Score.device('Window').grabFrame(${FRAME}, "${OUT}/frame$n.png");
JS
  SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
    timeout 120 "$SCORE" --no-gui --no-restore --script "$OUT/run$n.js" --wait 2 \
    > "$OUT/run$n.log" 2>&1
  return $?
}

echo "== rendering $CASE frame $FRAME, twice, in separate processes =="
render 1; rc1=$?
render 2; rc2=$?

fail=0
for n in 1 2; do
  if [ ! -s "$OUT/frame$n.png" ]; then
    echo "FAIL: run $n produced no frame (exit $([ $n = 1 ] && echo $rc1 || echo $rc2))"
    sed 's/^/    /' "$OUT/run$n.log" | tail -20
    fail=1
  fi
done
[ $fail -eq 1 ] && exit 1

if cmp -s "$OUT/frame1.png" "$OUT/frame2.png"; then
  echo "PASS: both runs produced byte-identical frames ($(stat -c%s "$OUT/frame1.png") bytes)"
  exit 0
fi

echo "FAIL: frame $FRAME differs between two runs of the same case."
echo "  run 1: $(stat -c%s "$OUT/frame1.png") bytes  md5 $(md5sum < "$OUT/frame1.png" | cut -d' ' -f1)"
echo "  run 2: $(stat -c%s "$OUT/frame2.png") bytes  md5 $(md5sum < "$OUT/frame2.png" | cut -d' ' -f1)"
echo "  Frame stepping is not deterministic yet; golden references cannot be pinned."
cp "$OUT/frame1.png" "$OUT/frame2.png" "${TMPDIR:-/tmp}/" 2>/dev/null && \
  echo "  copies kept in ${TMPDIR:-/tmp}/frame1.png and frame2.png"
exit 1
