#!/usr/bin/env bash
# Headless playback regressions.
#
#   tests/integration/headless-end/headless-end.sh
#
# "end" (BUG-LEDGER X8): plays a 3 s document with --no-gui --autoplay, checks
# it is playing 2 s in and stopped well after its end, and that the app is
# still running and exits 0 on OSC /exit. The end of playback looked up
# Actions::Stop, which only exists with a GUI, and the app aborted
# (std::out_of_range, exit 134).
#
# "resize" (BUG-LEDGER X7): resizes the root to 60 s while it plays, and checks
# it is still playing 25 s in. Execution received the stale stored max (the
# new-document 15.75 s, under the infinite flag) and ended playback there.
#
# Environment: OSSIA_SCORE (default build-developer/ossia-score), OUT.
# Self-serializes on flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"
BIN="${OSSIA_SCORE:-$SRCROOT/build-developer/ossia-score}"
OUT="${OUT:-/tmp/headless-end}"
OSC=6666

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }

rm -rf "$OUT"
send() { oscsend 127.0.0.1 $OSC "$@" 2>/dev/null; }

run_mode() { # mode -> $OUT/<mode>/
  local mode="$1" dir="$OUT/$1"
  mkdir -p "$dir/config-home/ossia"
  { printf 'var OUT_DIR = "%s";\nvar MODE = "%s";\n' "$dir" "$mode"; cat "$HERE/headless-end.js"; } > "$dir/scene.js"
  (
    # Well within the ctest TIMEOUT: a lock held that long is another
    # harness, and the test is skipped rather than killed.
    flock -w 100 9 || { echo 98 > "$dir/run.rc"; exit 0; }
    for _ in $(seq 1 60); do
      ss -Hlun "sport = :$OSC" 2>/dev/null | grep -q . && { sleep 1; continue; }
      break
    done
    env XDG_CONFIG_HOME="$dir/config-home" QT_QPA_PLATFORM=offscreen \
        SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
      timeout --foreground 120 "$BIN" --no-gui --no-restore \
        --script "$dir/scene.js" --wait 1 --autoplay >"$dir/run.log" 2>&1 &
    local APP=$!

    local ok=0
    for _ in $(seq 1 60); do [ -s "$dir/headless-end-init.score" ] && { ok=1; break; }; sleep 1; done
    if [ "$ok" = 0 ]; then
      kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$dir/run.rc"; exit 0
    fi

    if [ "$mode" = resize ]; then
      sleep 3                       # playback runs with the new-document durations
      send /script s "resizeRoot()"
      sleep 22                      # well past the stale 15.75 s max
      send /script s "checkPlaying()"
      sleep 1
    else
      sleep 2                       # about 2 s into the 3 s document
      send /script s "checkStarted()"
      sleep 8                       # it has ended well before this
      send /script s "checkStopped()"
      sleep 1
    fi
    if kill -0 "$APP" 2>/dev/null; then echo alive > "$dir/alive"; fi

    send /script s "finalizeRun()"
    sleep 1
    send /exit s force
    wait "$APP"; echo $? > "$dir/run.rc"
  ) 9>/tmp/score-harness.lock
}

FAILS=""
SKIPPED=""
for mode in end resize; do
  run_mode "$mode"
  dir="$OUT/$mode"
  rc=$(cat "$dir/run.rc" 2>/dev/null || echo 97)
  if [ "$rc" = 98 ]; then
    SKIPPED+=" $mode"
    continue
  fi
  [ -f "$dir/alive" ] || FAILS+=" $mode:DIED"
  [ "$rc" = 0 ] || FAILS+=" $mode:exit=$rc"
  grep -q "terminate called\|out_of_range" "$dir/run.log" && FAILS+=" $mode:ABORT"
  if [ "$mode" = end ]; then
    [ -s "$dir/started.score" ] || FAILS+=" end:NEVER-PLAYED"
    [ -s "$dir/stopped.score" ] || FAILS+=" end:NOT-STOPPED"
  fi
  if [ "$mode" = resize ] && [ ! -s "$dir/still-playing.score" ]; then
    FAILS+=" resize:STOPPED-AT-STALE-MAX"
  fi
done

if [ -z "$FAILS" ] && [ -n "$SKIPPED" ]; then
  echo "SKIP: /tmp/score-harness.lock busy for:$SKIPPED"; exit 77
elif [ -z "$FAILS" ]; then
  echo "headless-end PASS"
else
  echo "headless-end FAIL:$FAILS (out=$OUT)"; exit 1
fi
