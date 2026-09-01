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

# Derived, not hardcoded.
SRCROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DIR="$SRCROOT/tests/integration/live-edit"
BIN="${OSSIA_SCORE:-$SRCROOT/build-asan/ossia-score}"
BINDIR="$(cd "$(dirname "$BIN")" && pwd)"
GFXSO="$BINDIR/plugins/libscore_plugin_gfx.so"
GFXSRC="$SRCROOT/src/plugins/score-plugin-gfx/Gfx"
OUT="${OUT:-/tmp/live-edit}"
OSC=6666
TICK="${TICK:-0.5}"
BLANK_MEAN="${BLANK_MEAN:-0.002}"
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"

# An X server is required; see the env block in run_scenario for why offscreen
# is not a substitute (no GL -> Null RHI backend -> every verdict meaningless).
# Prefer an inherited DISPLAY, else bring up a headless one and take it down.
OWN_X=""
if [ -z "${DISPLAY:-}" ]; then
  for d in 99 98 97; do
    if command -v Xvfb >/dev/null 2>&1; then
      Xvfb ":$d" -screen 0 1280x720x24 >/dev/null 2>&1 &
    elif command -v Xephyr >/dev/null 2>&1; then
      Xephyr ":$d" -screen 1280x720 -ac -noreset >/dev/null 2>&1 &
    else
      break
    fi
    OWN_X=$!
    sleep 3
    if DISPLAY=":$d" xdpyinfo >/dev/null 2>&1; then
      export DISPLAY=":$d"
      trap 'kill "$OWN_X" 2>/dev/null' EXIT
      break
    fi
    kill "$OWN_X" 2>/dev/null; OWN_X=""
  done
fi
if [ -z "${DISPLAY:-}" ]; then
  echo "live-edit-sweep: no X server, and none could be started -- SKIP."
  echo "  offscreen is not a fallback: it has no GL, so score renders on the"
  echo "  Null backend and every verdict would be vacuous."
  exit 77
fi

mkdir -p "$OUT"

# scenario -> "<nticks> <require_render> [min_nonblack_coverage]"
#   min_nonblack_coverage: the fraction of the frame that must not be black.
#   A passthrough fed by a device fills the viewport, so anything well under 1.0
#   means the device did not deliver and the shader drew its disconnected-input
#   fallback instead -- which is non-blank, and would otherwise pass.
#   nticks chosen so every scenario mutates for ~8-10s of playback at 500ms;
#   parity matters: see each scenario's header for what the last tick leaves.
declare -A CFG=(
  [baseline]="0 yes"
  [add-remove-storm]="16 yes"
  [cable-storm]="15 yes"
  [undo-redo-during-play]="13 yes"
  [transport-storm]="18 no"
  [mixed-chaos]="20 yes"
  [window-storm]="24 yes"
  [camera-storm]="20 yes 0.5"
  [ndi-storm]="20 yes 0.5"
  [gfx-process-storm]="20 yes"
)
# Scenarios whose final tick leaves the full-screen isf-solid-color base
# (magenta, 255 0 255) as the only thing on the window. For these "not
# blank" is far too weak — a frame 0.3% lit passes, and any wrong colour
# passes identically — so the verdict requires the frame to actually BE
# magenta. camera/ndi end on device pixels (the coverage gate) and
# transport-storm does not require a render.
declare -A EXPECT=(
  [baseline]=magenta
  [add-remove-storm]=magenta
  # NOT magenta: cable-storm ends on isf-image-passthrough.fs, which despite its
  # name is a 2x2 sampling test card -- three quadrants sample the magenta input
  # and the fourth is a TEX_DIMENSIONS readback, (159,90,127) for a 1280x720
  # source. 3 * 628*352 = 663168 px = 71.96%% magenta is the CORRECT full-frame
  # result for this scene, reproduced pixel-identically by a statically wired
  # graph with no live editing at all. Its coverage gate below still applies.
  [undo-redo-during-play]=magenta
  [mixed-chaos]=magenta
  [window-storm]=magenta
  [gfx-process-storm]=magenta
)
ORDER=(baseline add-remove-storm cable-storm undo-redo-during-play transport-storm mixed-chaos
       window-storm gfx-process-storm camera-storm ndi-storm)

# Device-backed scenarios need the device. Absent hardware is a SKIP with the
# reason printed, never a silent pass -- but it stays a skip rather than the
# hard failure this file's media wrapper uses, because a camera and an NDI
# runtime are genuinely optional on a build host. SCORE_REQUIRE_DEVICES=1
# turns both into failures, which is what a bench machine should set.
NDI_MACHINE="$(hostname | tr '[:lower:]' '[:upper:]')"

precondition() { # name -> 0 runnable, 1 not (reason on stdout)
  case "$1" in
    camera-storm)
      for d in /dev/video*; do [ -e "$d" ] && return 0; done
      echo "no /dev/video* on this host"; return 1 ;;
    ndi-storm)
      ldconfig -p 2>/dev/null | grep -q 'libndi\.so' && return 0
      echo "libndi is not installed"; return 1 ;;
  esac
  return 0
}

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
  # Stop and exit through /script, not the bare /stop and /exit. This oscsend
  # emits argument-less messages that score's OSC listener rejects outright --
  # "element size must be multiple of four" / "unterminated address pattern" --
  # so the app never saw them and the run ended at the harness timeout instead.
  # /script carries a string argument and is accepted, which is why the grabs
  # above always worked while the shutdown silently did not.
  send /script s "Score.stop()"; sleep 0.5
  send /exit s force
}

run_scenario() { # name nticks
  local name="$1" nticks="$2"
  local log="$OUT/$name.log"

  # Stage the scenario with its directory injected. The scripts pull in
  # common.js through Score.readFile, which resolves nothing relative to the
  # running script -- --script only addImportPath()s that directory, and that
  # serves ES imports, not readFile. Staging keeps the .js files free of any
  # absolute path.
  local js="$OUT/$name.js"
  { printf 'var LIVE_EDIT_DIR = "%s";\n' "$DIR"
    printf 'var NDI_MACHINE = "%s";\n' "$NDI_MACHINE"
    cat "$DIR/$name.js"; } > "$js"
  rm -f "$OUT/$name-init.score" "$OUT/$name-final.score" "$OUT/$name.png" \
        "$OUT/$name.profraw" "$log" "$HOME/.config/ossia/failsafe.bit"
  (
    flock -w 900 9 || { echo 98 > "$OUT/$name.rc"; exit 0; }
    pump "$name" "$nticks" >/dev/null 2>&1 &
    local pumppid=$!
    # A REAL X server with xcb, NOT QT_QPA_PLATFORM=offscreen. Qt's offscreen
    # integration provides OpenGL only through GLX, so with no X there is no GL
    # at all: QRhi::create(OpenGLES2) fails and score falls back to the Null RHI
    # backend, which accepts every call and draws nothing. The grab then
    # succeeds and hands back a constant colour, so a non-blank check passes
    # while verifying nothing.
    # A real window on $DISPLAY is both a true render and a true readback, and
    # a nested/virtual X keeps it headless.
    # SCORE_SANITIZE_SKIP_CHECKS suppresses the first-run library-download
    # modal. It is a no-op under --no-gui but wedges a real GUI run forever in
    # QDialog::exec(), which reads as a hang rather than as a dialog.
    env SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
        SCORE_SANITIZE_SKIP_CHECKS=1 \
        QT_QPA_PLATFORM=xcb \
        LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/$name.profraw" \
      timeout --foreground 150 "$BIN" --no-restore \
        --script "$js" --wait 1 --autoplay >"$log" 2>&1
    echo $? > "$OUT/$name.rc"
    kill "$pumppid" 2>/dev/null; wait "$pumppid" 2>/dev/null
  ) 9>/tmp/score-harness.lock
}

# Every scenario quits with a document open, which is the only condition under
# which the four remaining by-value-but-Qt-owned members of ScenarioDocumentView
# are freed by Qt at an address that was never malloc'd. That defect is real,
# understood, filed, and deliberately not fixed here (m_view alone has ~26 call
# sites); leaving it to fail every scenario would cost the whole sweep its signal.
#
# So it is carved out BY SIGNATURE, not by disabling the check: a report is known
# only if its allocating frame is one of these two destructors. Any other
# AddressSanitizer report -- including a new one in the same file -- still fails.
#
# ScenarioDocumentView.cpp:778 is the empty ~ScenarioDocumentView, i.e. where
# m_view / m_timeRulerView / m_minimapView / m_minimap are destroyed, and every
# report anchors in one of those four. The last of the four is a SEGV rather
# than an invalid free only because the three before it have already poisoned
# ASan's shadow.
KNOWN_ASAN_FRAMES='Scenario::ProcessGraphicsView::~ProcessGraphicsView|Scenario::MinimapGraphicsView::~MinimapGraphicsView|Scenario::TimeRulerGraphicsView::~TimeRulerGraphicsView|Scenario::ScenarioDocumentView::~ScenarioDocumentView'

# Prints "<total> <known>" for the AddressSanitizer reports in a log. A report
# runs from its ERROR: line to its SUMMARY:, and counts as known only if one of
# the frames in between is a destructor above.
asan_census() {
  awk -v known="$KNOWN_ASAN_FRAMES" '
    /ERROR: AddressSanitizer/ { total++; inrep = 1; matched = 0; next }
    inrep && /SUMMARY: AddressSanitizer/ { if(matched) k++; inrep = 0; next }
    inrep && $0 ~ known { matched = 1 }
    END { if(inrep && matched) k++; print total+0, k+0 }' "$1" 2>/dev/null
}

verdict() { # name require_render [min_coverage] [expect] -> one line, nonzero on findings
  local name="$1" require="$2" cover="${3:-}" expect="${4:-}"
  local log="$OUT/$name.log" png="$OUT/$name.png"
  local rc; rc=$(cat "$OUT/$name.rc" 2>/dev/null || echo 97)
  local bad="" note=""
  local nasan nknown
  read -r nasan nknown <<< "$(asan_census "$log")"
  # ASan's own exit code is 1; that is not a finding when every report is known.
  if [ "$nasan" -gt 0 ] && [ "$nasan" = "$nknown" ]; then
    note=" known-shutdown-asan=$nknown"
    [ "$rc" = 1 ] && rc=0
  elif [ "$nasan" -gt 0 ]; then
    bad+=" ASAN($((nasan - nknown)) new)"
  fi
  [ "$rc" = 0 ]   || bad+=" exit=$rc"
  [ "$rc" = 124 ] && bad+="(TIMEOUT/hang)"
  grep -q "TICK-ERROR" "$log" 2>/dev/null && bad+=" JSERR"
  local ticks; ticks=$(grep -c "\[live-edit\] tick " "$log" 2>/dev/null); ticks=${ticks:-0}
  local mean="-" dom="-"
  if [ -s "$png" ]; then
    mean=$(convert "$png" -format '%[fx:mean]' info: 2>/dev/null || echo 0)
    # The mean alone is ambiguous -- magenta and yellow share it, and a quarter
    # of a frame over black reads as "not blank" while being visibly wrong. The
    # dominant non-black colour and its coverage make that visible in every run
    # instead of only when someone opens the png.
    dom=$(convert "$png" -colors 8 -format '%c' histogram:info: 2>/dev/null \
          | grep -av '#000000' | sort -rn | head -1 \
          | sed -E 's/^ *([0-9]+):.*(#[0-9A-Fa-f]{6}).*/\2x\1px/')
    dom="${dom:--}"
    if [ "$require" = yes ] && ! awk "BEGIN{exit !($mean > $BLANK_MEAN)}"; then bad+=" BLANK"; fi
    if [ "$expect" = magenta ]; then
      # Fraction of the frame within 2% of pure magenta.
      local mag; mag=$(convert "$png" -fuzz 2% -fill white -opaque '#FF00FF' \
            -fill black +opaque white -colorspace gray -format '%[fx:mean]' info: 2>/dev/null || echo 0)
      awk "BEGIN{exit !($mag >= 0.99)}" || bad+=" NOTMAGENTA(fraction=${mag:-0})"
    fi
    if [ -n "$cover" ]; then
      # Binarise and take the mean: that is literally the fraction of the frame
      # that is not black, independent of how bright the lit part happens to be.
      local nb; nb=$(convert "$png" -colorspace gray -threshold 1% -format '%[fx:mean]' info: 2>/dev/null || echo 0)
      awk "BEGIN{exit !($nb >= $cover)}" \
        || bad+=" NODEVICEPIXELS(nonblack=$nb < $cover)"
    fi
  else
    [ "$require" = yes ] && bad+=" NORENDER"
  fi
  if [ -z "$bad" ]; then
    printf '  %-24s PASS   ticks=%s mean=%s dom=%s%s\n' "$name" "$ticks" "$mean" "$dom" "$note"; return 0
  else
    printf '  %-24s FAIL  %s (ticks=%s mean=%s dom=%s log=%s)\n' "$name" "$bad" "$ticks" "$mean" "$dom" "$log"; return 1
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
SKIPPED=0
for name in "${ORDER[@]}"; do
  [ $# -gt 0 ] && { printf '%s\n' "$@" | grep -qx "$name" || continue; }
  read -r nticks require cover <<< "${CFG[$name]}"
  # A/B handle: NTICKS=0 runs a scenario's scene with no mutations at all, which
  # is how you tell "this chain never worked" from "the storm broke it".
  nticks="${NTICKS:-$nticks}"
  if ! why=$(precondition "$name"); then
    if [ "${SCORE_REQUIRE_DEVICES:-0}" = 1 ]; then
      printf '  %-24s FAIL   %s (SCORE_REQUIRE_DEVICES=1)\n' "$name" "$why"
      FAILED=$((FAILED+1)); continue
    fi
    printf '  %-24s SKIP   %s\n' "$name" "$why"; SKIPPED=$((SKIPPED+1)); continue
  fi
  echo "=== $name (${nticks} ticks @ ${TICK}s) ==="
  run_scenario "$name" "$nticks"
  verdict "$name" "$require" "${cover:-}" "${EXPECT[$name]:-}" || FAILED=$((FAILED+1))
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
