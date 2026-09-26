#!/usr/bin/env bash
# Headless playback regressions.
#
#   tests/integration/headless-end/headless-end.sh
#
# "end": plays a 3 s document with --no-gui --autoplay, sees it playing, then
# stopped at its end, and checks that the app is still running and exits 0 on
# OSC /exit: the end of playback must not need Actions::Stop, which only exists
# with a GUI.
#
# "resize": resizes the root to 60 s while it plays, and sees it still playing
# 18 s in, past the stale stored max (the new-document 15.75 s, under the
# infinite flag).
#
# Each state is polled over OSC until it is reached or its deadline passes.
#
# Environment: OSSIA_SCORE (default build-developer/ossia-score), OUT,
# SCORE_LOCAL_OSC_PORT. Self-serializes on the shared harness flock.
set -uo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"
BIN="${OSSIA_SCORE:-$SRCROOT/build-developer/ossia-score}"
OUT="${OUT:-/tmp/headless-end}"
OSC=${SCORE_LOCAL_OSC_PORT:-6666}

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }

rm -rf "$OUT"
send() { oscsend 127.0.0.1 "$OSC" "$@" 2>/dev/null; }

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

    # poll <js call> <marker file> <seconds>: sends the call until the
    # scene writes the marker, the app dies or the deadline passes.
    poll() {
      local deadline=$((SECONDS + $3))
      while [ "$SECONDS" -lt "$deadline" ]; do
        [ -s "$2" ] && return 0
        kill -0 "$APP" 2>/dev/null || return 1
        [ -n "$1" ] && send /script s "$1"
        sleep 0.2
      done
      [ -s "$2" ]
    }

    if ! poll "" "$dir/headless-end-init.score" 60; then
      kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$dir/run.rc"; exit 0
    fi

    if [ "$mode" = resize ]; then
      poll "checkStarted()" "$dir/started.score" 30 \
        && send /script s "resizeRoot()" \
        && poll "checkPlaying()" "$dir/still-playing.score" 60
    else
      poll "checkStarted()" "$dir/started.score" 30 \
        && poll "checkStopped()" "$dir/stopped.score" 30
      # What happens right after the end is what is under test.
      sleep 2
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
  [ -s "$dir/started.score" ] || FAILS+=" $mode:NEVER-PLAYED"
  if [ "$mode" = end ]; then
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
