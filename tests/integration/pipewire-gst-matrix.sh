#!/usr/bin/env bash
# score <-> gstreamer over PipeWire, across the axes that actually decide
# whether a link forms: pixel format, transport, and what the peer asks for.
#
# The round-trip harness covers score talking to itself and one canned
# gstreamer pipeline. This is the other half: the shapes a user writes by hand,
# which is where the format negotiation gets it wrong -- an output that pinned
# its framerate refused every consumer that asked for a different one, and said
# nothing except "no more output formats".
#
#   pipewire-gst-matrix.sh [--seconds N] [--only SUBSTR]
#
# Needs gst-launch-1.0, a live PipeWire daemon, and PipewirePublish from the
# build tree ($SCORE_BUILD, or the directory this was launched from).
set -u

SECONDS_PER=${SECONDS_PER:-6}
FILTER=""
while [ $# -gt 0 ]; do
  case "$1" in
    --seconds) SECONDS_PER="$2"; shift 2;;
    --only) FILTER="$2"; shift 2;;
    *) echo "unknown argument: $1" >&2; exit 2;;
  esac
done

BUILD="${SCORE_BUILD:-$PWD}"
PUBLISH="$BUILD/PipewirePublish"
GST=$(command -v gst-launch-1.0 || true)

[ -x "$PUBLISH" ] || { echo "SKIP: no PipewirePublish in $BUILD"; exit 77; }
[ -n "$GST" ] || { echo "SKIP: no gst-launch-1.0"; exit 77; }
pw-cli info 0 >/dev/null 2>&1 || { echo "SKIP: no PipeWire daemon"; exit 77; }

NODE=score-gst-matrix
TMP=$(mktemp -d)
pass=0; fail=0; skip=0
declare -a ROWS

cleanup() {
  [ -n "${PUB_PID:-}" ] && kill "$PUB_PID" 2>/dev/null
  rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

# Bring a score output up and wait for the node to exist.
PUB_PID=""
start_publisher() { # <format> <WxH> <dmabuf:0|1>
  # Wait for the previous node to actually leave the graph: grepping for the
  # name while the old one is still dying finds it and the consumer then
  # connects to a node that is on its way out.
  if [ -n "$PUB_PID" ]; then
    kill "$PUB_PID" 2>/dev/null
    wait "$PUB_PID" 2>/dev/null
    PUB_PID=""
    local j=0
    while [ $j -lt 50 ] && pw-dump 2>/dev/null | grep -q "\"$NODE\""; do
      sleep 0.1; j=$((j+1))
    done
  fi

  local extra=""
  [ "$3" = "1" ] && extra="--dmabuf"
  "$PUBLISH" "$NODE" "$2" "--format=$1" $extra >"$TMP/pub.log" 2>&1 &
  PUB_PID=$!

  local i=0
  while [ $i -lt 80 ]; do
    kill -0 "$PUB_PID" 2>/dev/null || { PUB_PID=""; return 1; }
    if pw-dump 2>/dev/null | grep -q "\"$NODE\""; then
      sleep 0.3   # let the stream finish connecting before a consumer arrives
      return 0
    fi
    sleep 0.1; i=$((i+1))
  done
  return 1
}

# Run a consumer pipeline. Prints the frame count it rendered, and leaves
# whatever went wrong in $TMP/gst.log for the caller to quote.
consume() { # <caps-or-empty> <converter chain>
  local caps="$1" chain="$2"
  local desc="pipewiresrc target-object=$NODE do-timestamp=true"
  [ -n "$caps" ] && desc="$desc ! $caps"
  desc="$desc ! $chain ! fpsdisplaysink video-sink=fakesink text-overlay=false sync=false"
  # shellcheck disable=SC2086
  timeout "$SECONDS_PER" $GST -v $desc >"$TMP/gst.log" 2>&1
  grep -oE "rendered: [0-9]+" "$TMP/gst.log" | tail -1 | grep -oE "[0-9]+"
}

# The first line of a gstreamer failure, which is the only informative one.
why() {
  grep -oE "stream error: [^\"]*|reason [a-z-]*" "$TMP/gst.log" 2>/dev/null \
    | head -1 || true
}

check() { # <cell> <frames>
  if [ -n "${2:-}" ] && [ "$2" -gt 0 ] 2>/dev/null; then
    row "$1" PASS "$2 frames"
  else
    row "$1" FAIL "$(why)"
  fi
}

row() { # <cell> <verdict> <detail>
  ROWS+=("$(printf '%-46s %-8s %s' "$1" "$2" "${3:-}")")
  case "$2" in
    PASS) pass=$((pass+1));; FAIL) fail=$((fail+1));; *) skip=$((skip+1));;
  esac
}

want() { [ -z "$FILTER" ] || case "$1" in *"$FILTER"*) return 0;; *) return 1;; esac; }

# --- score -> gstreamer -----------------------------------------------------
# Every format score can render into, over both transports, read back by a
# consumer that names the format and one that does not.
for fmt in rgba8 bgra8 rgb10a2 bgr10a2 rgba16f rgba32f; do
  for tr in shm dmabuf; do
    dm=0; [ "$tr" = dmabuf ] && dm=1
    cell="out-$fmt-$tr"
    want "$cell" || continue
    if ! start_publisher "$fmt" 1280x720 "$dm"; then
      row "$cell" SKIP "$(tail -1 "$TMP/pub.log" 2>/dev/null)"
      continue
    fi
    # A consumer that asks for nothing.
    check "$cell-open" "$(consume "" "videoconvert")"

    # A consumer that asks for a different framerate than the output runs at:
    # the case that used to refuse to link at all.
    check "$cell-fps30" "$(consume "video/x-raw,framerate=30/1" "videoconvert")"

    # A consumer that wants a pixel format the output does not produce, with
    # videoconvert bridging it.
    check "$cell-to-i420" \
      "$(consume "video/x-raw" "videoconvert ! video/x-raw,format=I420 ! videoconvert")"
  done
done

# --- gstreamer -> score -----------------------------------------------------
# The input side is covered by PipewireRoundtrip's gst2s cell, which drives
# score's InputStream directly. Publishing here as well would only re-test
# gstreamer talking to itself.

printf '\n%-46s %-8s %s\n' "cell" "verdict" "detail"
printf '%s\n' "--------------------------------------------------------------------------"
for r in "${ROWS[@]}"; do printf '%s\n' "$r"; done
printf '\n%d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"

# Only the 8-bit formats survive a gstreamer consumer: its pipewire elements do
# not map SPA's 10-bit and float layouts, which score offers because it can
# render into them and its own input reads them back (the s2s cells of
# PipewireRoundtrip cover that). Refusing is the right answer -- publishing
# RGBA bytes under another format would be worse -- so those cells are recorded
# rather than counted against the run.
expected_red=0
for r in "${ROWS[@]}"; do
  case "$r" in
    *rgb10a2*FAIL*|*bgr10a2*FAIL*|*rgba16f*FAIL*|*rgba32f*FAIL*)
      expected_red=$((expected_red+1));;
  esac
done
if [ "$expected_red" -gt 0 ]; then
  printf '%d of the failures are the formats gstreamer cannot take\n' "$expected_red"
fi
[ $((fail - expected_red)) -eq 0 ]
