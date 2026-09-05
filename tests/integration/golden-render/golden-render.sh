#!/usr/bin/env bash
# Golden-image render regression harness.
#
#   golden-render.sh [--backend <class>] [--update-refs] [--cases "name ..."]
#                    [--keep-going] [--list-backends] [--rebless]
#
# Renders each pinned tests-scene pipeline (cases-<class>.txt, falling back to
# cases-llvmpipe.txt) through the real ossia-score binary and compares the
# grabbed frame against refs/<case>.png with compare.py.
#
# ---------------------------------------------------------------------------
# ONE GOLDEN PER CASE, NOT ONE PER BACKEND.
#
# refs/ used to be refs/<class>/, four blessed copies of the same picture. That
# arrangement can describe a regression but never condemn one: a golden blessed
# on llvmpipe says nothing about what NVIDIA renders, so a change that breaks
# only NVIDIA is measured against a reference that moved with it and the table
# stays green. The four sets were measured against each other before they were
# collapsed and agreed to within 2 code values out of 255 on every one of 90
# cross-backend pairs; the split was not encoding a real difference. See the
# compare.py docstring for the numbers and for why the gate is three axes and
# not a PSNR floor alone.
#
# Per-backend RUN STATE (which cases are unstable or blank ON THIS BACKEND, and
# what renderer produced the run) is still per-backend and lives in
# refs/.state/<class>/. That is an observation about a driver, not a golden.
# ---------------------------------------------------------------------------
#
#   check mode (default)  : ref must exist; verdict per case is
#                           PASS / FAIL / NOREF / NORENDER / WRONG-BACKEND /
#                           SKIP-UNSTABLE / SKIP-BLANK.
#                           Exit 0 iff no FAIL/NOREF/NORENDER/WRONG-BACKEND.
#                           On FAIL the actual, the golden and a diff image are
#                           written to $OUT/diff/ so a CI failure is
#                           diagnosable without a local reproduction.
#   --update-refs         : renders each case TWICE and accepts the ref only if
#                           the two runs agree (compare.py --profile self) and
#                           are non-blank. Disagreeing cases land in
#                           refs/.state/<class>/UNSTABLE.txt, blank ones in
#                           BLANK.txt. Because the golden is now shared, an
#                           update that would move an EXISTING golden outside
#                           the shared tolerance is refused unless --rebless is
#                           given: otherwise running --update-refs on whatever
#                           GPU happens to be at hand silently rebases the
#                           reference for every other backend.
#   --rebless             : allow --update-refs to replace a golden that the
#                           new render does not match. Deliberate, and the
#                           picture must be looked at.
#
# ---------------------------------------------------------------------------
# BACKEND IDENTITY IS ASSERTED, NOT ASSUMED.
#
# Every run sets QT_LOGGING_RULES=qt.rhi.general=true, which makes QRhi print
# the backend it actually got:
#   OpenGL: "qt.rhi.general: OpenGL VENDOR: ... RENDERER: ... VERSION: ..."
#   Vulkan: "qt.rhi.general: Initializing Vulkan ... Device: ..." / "Adapter: ..."
# The line is stored next to the PNG as <case>.renderer and matched against the
# class's expected regex. A mismatch is a HARD FAILURE (WRONG-BACKEND), never a
# quiet pass: on a box with both an NVIDIA driver and Mesa, a silent fallback to
# the wrong GPU/rasteriser is the exact way a "green" table ends up measuring
# something other than what it claims.
#
# Measured on ai-workstation-01 (2026-08-21, NVIDIA 595.84, Mesa 25.2.8):
#   * LIBGL_ALWAYS_SOFTWARE=1 alone does NOT give llvmpipe when libglvnd
#     resolves to the NVIDIA vendor library -- it silently keeps the GPU.
#     Forcing software GL needs __GLX_VENDOR_LIBRARY_NAME=mesa as well.
#   * QT_QPA_PLATFORM=offscreen must not appear in a results table: it yields a
#     degraded context. The "*-offscreen" classes exist only so CI has
#     something to run without an X server, and they assert their renderer too.
# ---------------------------------------------------------------------------
#
# Serialization: each app run holds flock /tmp/score-harness.lock (OSC port
# 6666 is global). Do NOT wrap this whole script in that lock (see the
# EXHAUSTIVE-TEST-PLAN consolidation note on self-deadlock).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"  # tests/integration/golden-render -> repo root
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
SCRIPTS="${SCRIPTS:-$HOME/Documents/ossia/score/packages/csf-examples/csf-testers/tests-scene/scripts}"
OSC=6666
BLANK_MEAN="${BLANK_MEAN:-0.002}"
TIMEOUT="${TIMEOUT:-90}"
GRABTRIES="${GRABTRIES:-25}"   # x2s poll for the grab (ASAN startup is slow)
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0"

BACKEND=llvmpipe
UPDATE=0
KEEPGOING=0
CASES_OVERRIDE=""
PROFILE=""
REBLESS=0

# ---- backend classes --------------------------------------------------------
# name | graphics API | env | regex the reported renderer MUST match
backend_api() {
  case "$1" in
    vulkan-*|*-vulkan|vk-*) echo Vulkan ;;
    *) echo OpenGL ;;
  esac
}

backend_env() {
  case "$1" in
    nvidia)
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb __GLX_VENDOR_LIBRARY_NAME=nvidia" ;;
    nvidia-vulkan)
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb QT_VK_PHYSICAL_DEVICE_INDEX=${QT_VK_PHYSICAL_DEVICE_INDEX:-1}" ;;
    llvmpipe)
      # Real GLX against Mesa's software rasteriser. __GLX_VENDOR_LIBRARY_NAME
      # is mandatory: without it libglvnd hands us the NVIDIA GPU and
      # LIBGL_ALWAYS_SOFTWARE is silently ignored (measured 2026-08-21).
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe" ;;
    vulkan-lavapipe)
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb VK_LOADER_DRIVERS_SELECT=lvp*" ;;
    llvmpipe-offscreen)
      echo "QT_QPA_PLATFORM=offscreen __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe" ;;
    *)
      echo "" ;;
  esac
}

backend_expect() {
  case "$1" in
    nvidia|nvidia-vulkan)          echo 'NVIDIA|Quadro|GeForce|RTX' ;;
    llvmpipe|llvmpipe-offscreen)   echo 'llvmpipe' ;;
    vulkan-lavapipe)               echo 'llvmpipe|lavapipe' ;;
    *)                             echo '.' ;;
  esac
}

KNOWN_BACKENDS="nvidia nvidia-vulkan llvmpipe vulkan-lavapipe llvmpipe-offscreen"

while [ $# -gt 0 ]; do
  case "$1" in
    --backend) BACKEND="$2"; shift 2 ;;
    --update-refs) UPDATE=1; shift ;;
    --cases) CASES_OVERRIDE="$2"; shift 2 ;;
    --keep-going) KEEPGOING=1; shift ;;
    --profile) PROFILE="$2"; shift 2 ;;
    --rebless) REBLESS=1; shift ;;
    --list-backends) echo "$KNOWN_BACKENDS" | tr ' ' '\n'; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

# (The old "--compare A B" mode compared refs/A against refs/B. With one shared
# golden there is no second ref tree to compare against -- the comparison it
# used to make is now what every ordinary run performs.)

# Prerequisites -> ctest SKIP (return 77) rather than a hard failure.
command -v oscsend  >/dev/null || { echo "SKIP: oscsend not found";        exit 77; }
command -v convert  >/dev/null || { echo "SKIP: ImageMagick not found";     exit 77; }
python3 -c 'import numpy, PIL, scipy' 2>/dev/null \
                               || { echo "SKIP: python3 numpy/PIL/scipy missing"; exit 77; }
[ -x "$BIN" ]                  || { echo "SKIP: $BIN not built";            exit 77; }
[ -d "$SCRIPTS" ]             || { echo "SKIP: corpus missing ($SCRIPTS)";  exit 77; }

# GOLDEN_ENV_OVERRIDE / GOLDEN_EXPECT_OVERRIDE exist so the backend-identity
# guard can be negative-controlled: point a class at the wrong env and the run
# must go red. A guard nobody has seen fail is not a guard.
BENV="${GOLDEN_ENV_OVERRIDE:-$(backend_env "$BACKEND")}"
[ -n "$BENV" ] || { echo "unknown backend class '$BACKEND' (known: $KNOWN_BACKENDS)" >&2; exit 2; }
EXPECT="${GOLDEN_EXPECT_OVERRIDE:-$(backend_expect "$BACKEND")}"
API="$(backend_api "$BACKEND")"

# Classes that drive a real X server need one.
case "$BENV" in
  *DISPLAY=:0*)
    [ -e /tmp/.X11-unix/X0 ] || { echo "SKIP: backend $BACKEND needs an X server on :0"; exit 77; } ;;
esac

# One golden set for every backend; per-backend run state beside it.
REFS="$HERE/refs"
STATE="$HERE/refs/.state/$BACKEND"
OUT="${OUT:-/tmp/golden-render/$BACKEND}"
# Failure artifacts land in the build/output dir, named after the case, so a
# red CI run carries its own evidence.
DIFFDIR="$OUT/diff"
mkdir -p "$REFS" "$STATE" "$OUT" "$DIFFDIR"

CASES_FILE="$HERE/cases-$BACKEND.txt"
[ -f "$CASES_FILE" ] || CASES_FILE="$HERE/cases-llvmpipe.txt"
if [ -n "$CASES_OVERRIDE" ]; then
  read -r -a CASES <<< "$CASES_OVERRIDE"
else
  mapfile -t CASES < <(grep -v '^\s*#' "$CASES_FILE" | grep -v '^\s*$')
fi

# ---- isolated, HERMETIC config home -----------------------------------------
# Written from scratch -- deliberately NOT copied from the user's score.conf.
# Only the library root is inherited, because the corpus resolves
# "<LIBRARY>:/packages/...".
CFG="$OUT/config-home"
rm -rf "$CFG"; mkdir -p "$CFG/ossia"
LIBROOT="${SCORE_LIBRARY_ROOT:-$(cd "$SCRIPTS/../../../../.." && pwd)}"
cat > "$CFG/ossia/score.conf" <<EOF
[Library]
RootPath=$LIBROOT

[score_plugin_gfx]
GraphicsApi=$API
HardwareDecode=None
DecodingThreads=1
Rate=60
Samples=1
VSync=false
Buffers=3

[score_plugin_engine]
Logging=false
LogLevel=Nothing

[RemoteControl]
AllowScripting=false
Enabled=false
EOF

pixel_mean() { convert "$1" -format '%[fx:mean]' info: 2>/dev/null || echo 0; }

# ---- renderer identity ------------------------------------------------------
# Pull the line QRhi prints at device-creation time out of a run log.
extract_renderer() { # log -> one line, or "" if QRhi never reported
  # OpenGL: a single line carries vendor+renderer+version.
  local gl
  gl=$(grep -m1 -E 'qt\.rhi\.general: OpenGL VENDOR:' "$1" 2>/dev/null | sed 's/^qt\.rhi\.general: //')
  [ -n "$gl" ] && { printf '%s' "$gl"; return; }
  # Vulkan / D3D / Metal: QRhi enumerates every device and then says which one
  # it kept on the NEXT line. Taking the first enumerated device instead would
  # be exactly the "we measured something else" bug this guard exists to catch:
  # on this box device 0 is an RTX 4090 while the selected one is the Quadro.
  awk '/qt\.rhi\.general: (Physical device|Adapter|Device) /{last=$0}
       /using this (physical device|adapter|device)/{sub(/^qt\.rhi\.general: /,"",last); print last; found=1; exit}
       END{if(!found) exit 1}' "$1" 2>/dev/null && return
  # score builds its own QVulkanInstance and device, so QRhi *imports* them
  # rather than enumerating: there is no "using this physical device"
  # confirmation line, only one "Using imported physical device '<name>' ..."
  # line. Not matching it made every Vulkan case report WRONG-BACKEND -- 15 of
  # 16 in the nvidia-vulkan class -- while the runs were in fact correct, on the
  # right GPU. The line still names the device, so the guard's intent (know what
  # produced the number) is preserved.
  grep -m1 -E 'qt\.rhi\.general: Using imported (physical device|device|adapter) ' "$1" 2>/dev/null \
    | sed 's/^qt\.rhi\.general: //'
}

# ---- one full app run -> one PNG --------------------------------------------
render_one() { # case_name out_png -> 0 ok, 2 no png, 3 wrong backend
  local name="$1" png="$2" js="$SCRIPTS/$1.js" log="$OUT/$1.log"
  rm -f "$png"
  [ -f "$js" ] || { echo "  missing script $js" >&2; return 2; }
  (
    flock -w 300 9 || { echo "  LOCK-TIMEOUT" >&2; exit 4; }
    ( for _ in $(seq 1 "$GRABTRIES"); do
        sleep 2
        oscsend 127.0.0.1 $OSC /script s "Score.device('Window').grabTo('$png')" 2>/dev/null
        [ -s "$png" ] && break
      done
      sleep 0.5; oscsend 127.0.0.1 $OSC /script s "Score.stop()"; sleep 0.5; oscsend 127.0.0.1 $OSC /exit s force ) >/dev/null 2>&1 &
    # shellcheck disable=SC2086
    env -u DISPLAY XDG_CONFIG_HOME="$CFG" \
        SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
        SCORE_FORCE_OFFSCREEN_WINDOW=Window \
        QT_LOGGING_RULES='qt.rhi.general=true' \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/%p.profraw" \
        $BENV \
      timeout --foreground "$TIMEOUT" "$BIN" --no-gui --no-restore \
        --script "$js" --wait 1 --autoplay >"$log" 2>&1
    wait 2>/dev/null
  ) 9>/tmp/score-harness.lock

  RENDERER="$(extract_renderer "$log")"
  printf '%s\n' "$RENDERER" > "$OUT/$name.renderer"
  # An unidentified backend is as bad as a wrong one: we would be publishing a
  # number without knowing what produced it.
  if [ -z "$RENDERER" ]; then WRONG="no qt.rhi device line in $log"; return 3; fi
  if ! printf '%s' "$RENDERER" | grep -qE "$EXPECT"; then
    WRONG="expected /$EXPECT/, got: $RENDERER"; return 3
  fi
  [ -s "$png" ] || return 2
  return 0
}

listed() { [ -f "$2" ] && grep -qx "$1" "$2"; }

# BROKEN.txt: "<case>  <why>" for cases that do not render on ANY backend
# because of a defect in score, not a property of the backend. They are skipped
# with the reason printed rather than left as a standing red, because a gate
# that is red on day one is a gate nobody looks at -- but the reason is printed
# on every single run so the defect cannot quietly become furniture. Delete the
# line the moment the case renders; --update-refs will then pick it up.
broken_reason() {
  [ -f "$HERE/BROKEN.txt" ] || return 1
  awk -v n="$1" '$1 == n { $1=""; sub(/^ +/,""); print; found=1 }
                 END { exit !found }' "$HERE/BROKEN.txt"
}

fails=0; passes=0; skips=0
RENDERER=""; WRONG=""
echo "backend=$BACKEND api=$API expect=/$EXPECT/"
echo "env: $BENV"
for name in "${CASES[@]}"; do
  printf '%-42s' "$name"
  if [ "$UPDATE" = 1 ]; then
    render_one "$name" "$OUT/$name.A.png"; rcA=$?
    if [ "$rcA" = 3 ]; then echo "WRONG-BACKEND ($WRONG)"; fails=$((fails+1)); continue; fi
    render_one "$name" "$OUT/$name.B.png"; rcB=$?
    if [ "$rcB" = 3 ]; then echo "WRONG-BACKEND ($WRONG)"; fails=$((fails+1)); continue; fi
    if [ "$rcA" = 0 ] && [ "$rcB" = 0 ]; then
      m=$(pixel_mean "$OUT/$name.A.png")
      if ! awk "BEGIN{exit !($m > $BLANK_MEAN)}"; then
        echo "BLANK mean=$m (not accepted as ref)"; grep -qx "$name" "$STATE/BLANK.txt" 2>/dev/null || echo "$name" >> "$STATE/BLANK.txt"
        skips=$((skips+1)); continue
      fi
      if res=$(python3 "$HERE/compare.py" "$OUT/$name.A.png" "$OUT/$name.B.png" --profile self); then
        # The golden is shared. Two self-consistent renders on THIS backend are
        # necessary but not sufficient to replace it: if an existing golden
        # disagrees with them, either this backend really is different (which
        # the whole point of a shared golden is to surface) or the code changed.
        # Both deserve a human, so refuse silently rebasing every other
        # backend's reference off whichever GPU ran the update.
        if [ -f "$REFS/$name.png" ] && [ "$REBLESS" = 0 ]; then
          if ! chk=$(python3 "$HERE/compare.py" "$REFS/$name.png" "$OUT/$name.A.png" \
                       --diff-dir "$DIFFDIR" --name "$name"); then
            echo "REF-CONFLICT ($chk) — existing golden kept; --rebless to replace"
            fails=$((fails+1)); continue
          fi
        fi
        cp "$OUT/$name.A.png" "$REFS/$name.png"
        printf '%s\n' "$RENDERER" > "$STATE/RENDERER.txt"
        # A reference image with no provenance is an assertion with no author:
        # when it later disagrees with a run, nobody can tell whether the code
        # regressed or the reference was made by a binary that no longer exists.
        {
          echo "date:     $(date -Iseconds)"
          echo "backend:  $BACKEND ($API)"
          echo "renderer: $RENDERER"
          echo "binary:   $BIN"
          echo "commit:   $(git -C "$SRCROOT" rev-parse HEAD 2>/dev/null || echo unknown)"
          echo "built:    $(stat -c %y "$BIN" 2>/dev/null || echo unknown)"
          echo "corpus:   $SCRIPTS"
        } > "$REFS/PROVENANCE.txt"
        # no longer unstable/blank if it stabilized
        sed -i "/^$name\$/d" "$STATE/UNSTABLE.txt" "$STATE/BLANK.txt" 2>/dev/null
        echo "REF-UPDATED ($res)"; passes=$((passes+1))
      else
        echo "UNSTABLE ($res) — excluded"; grep -qx "$name" "$STATE/UNSTABLE.txt" 2>/dev/null || echo "$name" >> "$STATE/UNSTABLE.txt"
        skips=$((skips+1))
      fi
    else
      echo "NORENDER (see $OUT/$name.log)"; fails=$((fails+1))
      [ "$KEEPGOING" = 1 ] || true
    fi
  else
    if reason=$(broken_reason "$name"); then
      echo "SKIP-BROKEN  $reason"; skips=$((skips+1)); continue; fi
    if listed "$name" "$STATE/UNSTABLE.txt"; then echo "SKIP-UNSTABLE"; skips=$((skips+1)); continue; fi
    if listed "$name" "$STATE/BLANK.txt"; then echo "SKIP-BLANK"; skips=$((skips+1)); continue; fi
    if [ ! -f "$REFS/$name.png" ]; then echo "NOREF (run --update-refs)"; fails=$((fails+1)); continue; fi
    render_one "$name" "$OUT/$name.png"; rc=$?
    if [ "$rc" = 3 ]; then
      echo "WRONG-BACKEND ($WRONG)"; fails=$((fails+1))
    elif [ "$rc" = 0 ]; then
      if res=$(python3 "$HERE/compare.py" "$REFS/$name.png" "$OUT/$name.png" \
                 --profile "${PROFILE:-shared}" --diff-dir "$DIFFDIR" --name "$name"); then
        echo "PASS ($res)"; passes=$((passes+1))
      else
        echo "FAIL ($res)"; fails=$((fails+1))
        echo "    golden=$REFS/$name.png"
        echo "    actual=$OUT/$name.png"
        echo "    artifacts=$DIFFDIR/$name.{golden,actual,diff}.png"
      fi
    else
      echo "NORENDER (see $OUT/$name.log)"; fails=$((fails+1))
    fi
  fi
done

# Intent assertions, reference-free: each case is checked against what its own
# shader header says it must look like. A reference only pins what the renderer
# DID -- a backend that quietly falls back and paints a constant produces a
# perfectly stable, perfectly reproducible, perfectly wrong reference, and two
# flipped refs in this very set passed the image gate for a whole campaign.
# Run in both modes: in --update-refs it gates what is allowed to become a ref.
content_rc=0
if [ -f "$HERE/assert-content.py" ]; then
  echo "----"
  if python3 "$HERE/assert-content.py" "$REFS"; then
    echo "assert-content[$BACKEND]: ok"
  else
    content_rc=1
    echo "assert-content[$BACKEND]: FAILED -- a reference disagrees with its shader header"
  fi
fi

echo "----"
echo "renderer: ${RENDERER:-(none reported)}"
echo "golden-render[$BACKEND]$([ "$UPDATE" = 1 ] && echo ' (update-refs)'): $passes ok, $fails failing, $skips skipped"
# In check mode, if there are no refs at all yet, SKIP (77) instead of failing —
# refs are generated once with --update-refs and committed alongside the branch.
if [ "$UPDATE" = 0 ] && [ "$passes" = 0 ] && [ "$fails" -gt 0 ] && ! ls "$REFS"/*.png >/dev/null 2>&1; then
  echo "SKIP: no references present — run --update-refs once and commit refs/"; exit 77
fi
[ "$fails" = 0 ] && [ "$content_rc" = 0 ]
