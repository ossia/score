#!/usr/bin/env bash
# Runs every tests-scene tester and records what came out.
#
# One process per case: a case that crashes or hangs takes only itself down, and
# the document each tester builds starts from a clean application either way.
#
#   sweep.sh [--score PATH] [--out DIR] [--frame N] [--filter GLOB]
#
# Writes DIR/results.tsv (case, verdict, colours, ms, note) and DIR/<case>.png.
# Verdicts:
#   RENDER    frame written, more than one colour in it
#   BLANK     frame written, every pixel identical
#   NORENDER  nothing wired to the Window device, or no frame written
#   SCREEN    the grab fell back to capturing the screen -- result is worthless
#   FAIL      score exited nonzero, or was killed by the guard
set -uo pipefail

SCORE="${OSSIA_SCORE:-}"
OUT="${SWEEP_OUT:-/tmp/score-sweep}"
FRAME=30
FILTER='*'
SCRIPTS="<LIBRARY>:/packages/csf-examples/csf-testers/tests-scene/scripts"
SCRIPTS_DIR="${SWEEP_SCRIPTS_DIR:-$HOME/Documents/ossia/score/packages/csf-examples/csf-testers/tests-scene/scripts}"

while [ $# -gt 0 ]; do
  case "$1" in
    --score)  SCORE="$2";  shift 2 ;;
    --out)    OUT="$2";    shift 2 ;;
    --frame)  FRAME="$2";  shift 2 ;;
    --filter) FILTER="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

[ -n "$SCORE" ] || { echo "FATAL: set OSSIA_SCORE or pass --score"; exit 2; }
[ -x "$SCORE" ] || { echo "FATAL: not executable: $SCORE"; exit 2; }
[ -d "$SCRIPTS_DIR" ] || { echo "FATAL: no scripts at $SCRIPTS_DIR"; exit 2; }

mkdir -p "$OUT/logs"
: > "$OUT/results.tsv"

colours() {
  python3 - "$1" <<'PY' 2>/dev/null || echo "?"
import sys
try:
    from PIL import Image
    print(len(set(Image.open(sys.argv[1]).convert('RGB').getdata())))
except Exception:
    print("?")
PY
}

total=0; render=0; blank=0; norender=0; failed=0; screen=0
start_all=$(date +%s)

for f in "$SCRIPTS_DIR"/build-$FILTER.js; do
  [ -e "$f" ] || continue
  name=$(basename "$f" .js)
  total=$((total + 1))
  png="$OUT/$name.png"
  log="$OUT/logs/$name.log"
  rm -f "$png"

  cat > "$OUT/run.js" <<JS
eval(Score.readFile("$SCRIPTS/${name}.js"));
Score.play();
Score.device('Window').grabFrame(${FRAME}, "${png}");
Qt.exit(0);
JS

  t0=$(date +%s%N)
  SCORE_FORCE_OFFSCREEN_WINDOW=Window \
  SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 DISPLAY="${DISPLAY:-:0}" \
    timeout 90 "$SCORE" --no-gui --no-restore --script "$OUT/run.js" --wait 0 --autoplay \
    > "$log" 2>&1
  rc=$?
  ms=$(( ($(date +%s%N) - t0) / 1000000 ))

  note=""
  if grep -q "capturing the SCREEN" "$log"; then
    verdict=SCREEN; screen=$((screen + 1))
  elif [ ! -s "$png" ]; then
    if grep -q "nothing rendered into" "$log"; then
      verdict=NORENDER; norender=$((norender + 1))
    elif [ $rc -ne 0 ]; then
      verdict=FAIL; failed=$((failed + 1)); note="exit $rc"
    else
      verdict=NORENDER; norender=$((norender + 1)); note="no frame written"
    fi
  else
    c=$(colours "$png")
    if [ "$c" = "1" ]; then verdict=BLANK; blank=$((blank + 1))
    else verdict=RENDER; render=$((render + 1)); fi
    note="$c colours"
  fi

  # First real complaint from the log, so the table says why without opening it.
  if [ -z "$note" ] || [ "$verdict" = FAIL ] || [ "$verdict" = NORENDER ]; then
    why=$(grep -m1 -E 'error|Error|failed|Failed|not supported|Missing' "$log" \
          | cut -c1-90 | tr -d '\t')
    [ -n "$why" ] && note="${note:+$note; }$why"
  fi

  printf '%s\t%s\t%s\t%s\n' "$name" "$verdict" "$ms" "$note" >> "$OUT/results.tsv"
  printf '  %-46s %-9s %5s ms  %s\n' "$name" "$verdict" "$ms" "$note"
done

echo
echo "== $total cases in $(( $(date +%s) - start_all ))s =="
printf '   RENDER %d   BLANK %d   NORENDER %d   FAIL %d   SCREEN %d\n' \
  "$render" "$blank" "$norender" "$failed" "$screen"
echo "   results: $OUT/results.tsv"

# SCREEN means the harness measured the desktop, which is never a usable result.
[ "$screen" -gt 0 ] && exit 1
exit 0
