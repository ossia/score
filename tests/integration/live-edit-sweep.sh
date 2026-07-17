#!/usr/bin/env bash
# Live graph mutation DURING execution — behavioral sweep.
#
#   tests/integration/live-edit-sweep.sh [scenario ...]     (default: all)
#
# Each scenario in tests/integration/live-edit/*.js builds a small ISF scene
# (Window device + solid-color ISF), which ossia-score autoplays headless on
# llvmpipe. This script then injects mutations WHILE IT PLAYS by sending
# `tick()` over OSC (/script s "tick()" on udp/6666) every TICK seconds —
# process add/remove storms, cable storms, undo/redo storms, transport
# storms — exercising GfxContext::recompute_graph / add_edge / remove_edge,
# Graph::recreateOutputRenderList and the execution engine's live-edit path.
#
# Verdict per scenario (teardown crash is fixed on this branch, so):
#   PASS  = exit code 0  AND  no "ERROR: AddressSanitizer" in the log
#           AND (where the scenario expects a live render) final grab
#           non-blank  AND  no TICK-ERROR (mutation actually happened).
#   Anything else is a FINDING, not flake — investigate the log.
#
# Runs under flock /tmp/score-harness.lock (OSC port 6666 is global).
# Each run writes LLVM_PROFILE_FILE=$OUT/<name>.profraw; after the sweep,
# per-scenario function coverage of GfxContext.cpp / Graph.cpp /
# RenderList.cpp is diffed against the no-mutation `baseline` scenario.
set -u

SRCROOT="/home/jcelerier/ossia/wt/score-tests"
DIR="$SRCROOT/tests/integration/live-edit"
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
GFXSO="$SRCROOT/build-sanitizers/plugins/libscore_plugin_gfx.so"
GFXSRC="$SRCROOT/src/plugins/score-plugin-gfx/Gfx"
OUT="${OUT:-/tmp/live-edit}"
OSC=6666
TICK="${TICK:-0.5}"
BLANK_MEAN="${BLANK_MEAN:-0.002}"
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"

mkdir -p "$OUT"

# scenario -> "<nticks> <require_render>"
#   nticks chosen so every scenario mutates for ~8-10s of playback at 500ms;
#   parity matters: see each scenario's header for what the last tick leaves.
declare -A CFG=(
  [baseline]="0 yes"
  [add-remove-storm]="16 yes"
  [cable-storm]="15 yes"
  [undo-redo-during-play]="13 yes"
  [transport-storm]="18 no"
  [mixed-chaos]="20 yes"
)
ORDER=(baseline add-remove-storm cable-storm undo-redo-during-play transport-storm mixed-chaos)

send() { oscsend 127.0.0.1 "$OSC" "$@" 2>/dev/null; }

pump() { # name nticks — runs alongside the app, under the same lock
  local name="$1" nticks="$2" png="$OUT/$name.png"
  # Wait until the scene is built (init marker saved by markReady) + play begun.
  for _ in $(seq 1 60); do [ -s "$OUT/$name-init.score" ] && break; sleep 0.5; done
  sleep 2
  local i
  for i in $(seq 1 "$nticks"); do send /script s "tick()"; sleep "$TICK"; done
  # Restore a known-rendering state, make sure the transport runs, grab.
  send /script s "tick_final()"; sleep 0.5
  send /script s "Score.play()"; sleep 1.5
  for _ in $(seq 1 15); do
    send /script s "Score.device('Window').grabTo('$png')"
    sleep 1; [ -s "$png" ] && break
  done
  send /script s "finalize('$name')"; sleep 1
  send /stop;  sleep 0.5
  send /exit
}

run_scenario() { # name nticks
  local name="$1" nticks="$2"
  local js="$DIR/$name.js" log="$OUT/$name.log"
  rm -f "$OUT/$name-init.score" "$OUT/$name-final.score" "$OUT/$name.png" \
        "$OUT/$name.profraw" "$log" "$HOME/.config/ossia/failsafe.bit"
  (
    flock -w 900 9 || { echo 98 > "$OUT/$name.rc"; exit 0; }
    pump "$name" "$nticks" >/dev/null 2>&1 &
    local pumppid=$!
    env SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
        SCORE_FORCE_OFFSCREEN_WINDOW=Window QT_QPA_PLATFORM=offscreen \
        LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/$name.profraw" \
      timeout --foreground 150 "$BIN" --no-gui --no-restore \
        --script "$js" --wait 1 --autoplay >"$log" 2>&1
    echo $? > "$OUT/$name.rc"
    kill "$pumppid" 2>/dev/null; wait "$pumppid" 2>/dev/null
  ) 9>/tmp/score-harness.lock
}

verdict() { # name require_render -> prints one line, returns nonzero on findings
  local name="$1" require="$2"
  local log="$OUT/$name.log" png="$OUT/$name.png"
  local rc; rc=$(cat "$OUT/$name.rc" 2>/dev/null || echo 97)
  local bad=""
  [ "$rc" = 0 ]   || bad+=" exit=$rc"
  [ "$rc" = 124 ] && bad+="(TIMEOUT/hang)"
  grep -q "ERROR: AddressSanitizer" "$log" 2>/dev/null && bad+=" ASAN"
  grep -q "TICK-ERROR" "$log" 2>/dev/null && bad+=" JSERR"
  local ticks; ticks=$(grep -c "\[live-edit\] tick " "$log" 2>/dev/null); ticks=${ticks:-0}
  local mean="-"
  if [ -s "$png" ]; then
    mean=$(convert "$png" -format '%[fx:mean]' info: 2>/dev/null || echo 0)
    if [ "$require" = yes ] && ! awk "BEGIN{exit !($mean > $BLANK_MEAN)}"; then bad+=" BLANK"; fi
  else
    [ "$require" = yes ] && bad+=" NORENDER"
  fi
  if [ -z "$bad" ]; then
    printf '  %-24s PASS   ticks=%s mean=%s\n' "$name" "$ticks" "$mean"; return 0
  else
    printf '  %-24s FAIL  %s (ticks=%s mean=%s log=%s)\n' "$name" "$bad" "$ticks" "$mean" "$log"; return 1
  fi
}

coverage() { # name — list of gfx functions with >0 region coverage
  local name="$1"
  [ -s "$OUT/$name.profraw" ] || return 0
  llvm-profdata-20 merge -sparse "$OUT/$name.profraw" -o "$OUT/$name.profdata" 2>/dev/null || return 0
  llvm-cov-20 report "$BIN" -object "$GFXSO" -instr-profile="$OUT/$name.profdata" \
      -show-functions -Xdemangler c++filt \
      "$GFXSRC/GfxContext.cpp" "$GFXSRC/Graph/Graph.cpp" "$GFXSRC/Graph/RenderList.cpp" \
      > "$OUT/$name.functions.txt" 2>/dev/null
  # Rows: <demangled name (may contain spaces)> then numeric column groups
  # (Regions Miss Cover% [Lines Miss Cover% [Branches Miss Cover%]]).
  # The FIRST %-field is region coverage; the name is everything before the
  # two counters preceding it.
  awk '{
         p=0; for(i=1;i<=NF;i++) if($i ~ /%$/){p=i;break}
         if(p<3) next
         if($p == "0.00%") next
         name=""; for(i=1;i<=p-3;i++) name = name (i>1?" ":"") $i
         if(name != "" && name !~ /^(---|Filename|TOTAL|File)/) print name
       }' "$OUT/$name.functions.txt" | sort -u > "$OUT/$name.hit.txt"
}

FAILED=0
for name in "${ORDER[@]}"; do
  [ $# -gt 0 ] && { printf '%s\n' "$@" | grep -qx "$name" || continue; }
  read -r nticks require <<< "${CFG[$name]}"
  echo "=== $name (${nticks} ticks @ ${TICK}s) ==="
  run_scenario "$name" "$nticks"
  verdict "$name" "$require" || FAILED=$((FAILED+1))
  coverage "$name"
done

# Coverage delta vs baseline: which GfxContext/Graph/RenderList functions
# does live mutation light up that plain playback does not?
if [ -s "$OUT/baseline.hit.txt" ]; then
  echo
  echo "=== gfx functions newly hit vs baseline (GfxContext/Graph/RenderList) ==="
  for name in "${ORDER[@]}"; do
    [ "$name" = baseline ] && continue
    [ -s "$OUT/$name.hit.txt" ] || continue
    local_new=$(comm -13 "$OUT/baseline.hit.txt" "$OUT/$name.hit.txt")
    n=$(printf '%s' "$local_new" | grep -c . || true)
    echo "--- $name (+$n) ---"
    [ -n "$local_new" ] && printf '%s\n' "$local_new" | sed 's/^/    /'
  done
fi

echo
echo "artifacts under $OUT/ (png, log, rc, .score, functions.txt, hit.txt)"
[ "$FAILED" = 0 ] || { echo "$FAILED scenario(s) FAILED — real findings, see logs"; exit 1; }
