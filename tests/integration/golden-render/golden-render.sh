#!/usr/bin/env bash
# Golden-image render regression harness.
#
#   golden-render.sh [--backend <class>] [--update-refs] [--cases "name ..."]
#                    [--keep-going] [--list-backends]
#   golden-render.sh --compare <backendA> <backendB> [--profile cross|strict|loose]
#
# Renders each pinned tests-scene pipeline (cases-<class>.txt, falling back to
# cases-llvmpipe.txt) through the real ossia-score binary and compares the
# grabbed frame against refs/<class>/<case>.png with compare.py.
#
#   check mode (default)  : ref must exist; verdict per case is
#                           PASS / FAIL / NOREF / NORENDER / WRONG-BACKEND /
#                           SKIP-UNSTABLE / SKIP-BLANK.
#                           Exit 0 iff no FAIL/NOREF/NORENDER/WRONG-BACKEND.
#   --update-refs         : renders each case TWICE and accepts the ref only if
#                           the two runs agree (compare.py --profile self) and
#                           are non-blank. Disagreeing cases land in
#                           refs/<class>/UNSTABLE.txt, blank ones in BLANK.txt.
#   --compare A B         : compares refs/A/*.png against refs/B/*.png with the
#                           given profile (default "cross"). No rendering.
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
COMPARE_A=""
COMPARE_B=""

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
    --compare) COMPARE_A="$2"; COMPARE_B="$3"; shift 3 ;;
    --list-backends) echo "$KNOWN_BACKENDS" | tr ' ' '\n'; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

# ---- cross-backend comparison mode -----------------------------------------
if [ -n "$COMPARE_A" ]; then
  P="${PROFILE:-cross}"
  A="$HERE/refs/$COMPARE_A" ; B="$HERE/refs/$COMPARE_B"
  [ -d "$A" ] || { echo "no refs for $COMPARE_A"; exit 2; }
  [ -d "$B" ] || { echo "no refs for $COMPARE_B"; exit 2; }
  echo "renderer[$COMPARE_A] = $(cat "$A/RENDERER.txt" 2>/dev/null || echo '(unrecorded)')"
  echo "renderer[$COMPARE_B] = $(cat "$B/RENDERER.txt" 2>/dev/null || echo '(unrecorded)')"
  echo "profile = $P"
  agree=0; differ=0; only=0
  for f in "$A"/*.png; do
    [ -e "$f" ] || continue
    n="$(basename "$f" .png)"
    printf '%-42s' "$n"
    if [ ! -f "$B/$n.png" ]; then echo "ONLY-IN-$COMPARE_A"; only=$((only+1)); continue; fi
    if res=$(python3 "$HERE/compare.py" "$f" "$B/$n.png" --profile "$P"); then
      echo "AGREE  ($res)"; agree=$((agree+1))
    else
      echo "DIFFER ($res)"; differ=$((differ+1))
    fi
  done
  for f in "$B"/*.png; do
    [ -e "$f" ] || continue
    n="$(basename "$f" .png)"
    [ -f "$A/$n.png" ] || { printf '%-42s%s\n' "$n" "ONLY-IN-$COMPARE_B"; only=$((only+1)); }
  done
  echo "----"
  echo "cross[$COMPARE_A vs $COMPARE_B] profile=$P: $agree agree, $differ differ, $only one-sided"
  exit 0
fi

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

REFS="$HERE/refs/$BACKEND"
OUT="${OUT:-/tmp/golden-render/$BACKEND}"
mkdir -p "$REFS" "$OUT"

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
        echo "BLANK mean=$m (not accepted as ref)"; grep -qx "$name" "$REFS/BLANK.txt" 2>/dev/null || echo "$name" >> "$REFS/BLANK.txt"
        skips=$((skips+1)); continue
      fi
      if res=$(python3 "$HERE/compare.py" "$OUT/$name.A.png" "$OUT/$name.B.png" --profile self); then
        cp "$OUT/$name.A.png" "$REFS/$name.png"
        printf '%s\n' "$RENDERER" > "$REFS/RENDERER.txt"
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
        sed -i "/^$name\$/d" "$REFS/UNSTABLE.txt" "$REFS/BLANK.txt" 2>/dev/null
        echo "REF-UPDATED ($res)"; passes=$((passes+1))
      else
        echo "UNSTABLE ($res) — excluded"; grep -qx "$name" "$REFS/UNSTABLE.txt" 2>/dev/null || echo "$name" >> "$REFS/UNSTABLE.txt"
        skips=$((skips+1))
      fi
    else
      echo "NORENDER (see $OUT/$name.log)"; fails=$((fails+1))
      [ "$KEEPGOING" = 1 ] || true
    fi
  else
    if reason=$(broken_reason "$name"); then
      echo "SKIP-BROKEN  $reason"; skips=$((skips+1)); continue; fi
    if listed "$name" "$REFS/UNSTABLE.txt"; then echo "SKIP-UNSTABLE"; skips=$((skips+1)); continue; fi
    if listed "$name" "$REFS/BLANK.txt"; then echo "SKIP-BLANK"; skips=$((skips+1)); continue; fi
    if [ ! -f "$REFS/$name.png" ]; then echo "NOREF (run --update-refs)"; fails=$((fails+1)); continue; fi
    render_one "$name" "$OUT/$name.png"; rc=$?
    if [ "$rc" = 3 ]; then
      echo "WRONG-BACKEND ($WRONG)"; fails=$((fails+1))
    elif [ "$rc" = 0 ]; then
      if res=$(python3 "$HERE/compare.py" "$REFS/$name.png" "$OUT/$name.png" --profile "${PROFILE:-strict}"); then
        echo "PASS ($res)"; passes=$((passes+1))
      else
        echo "FAIL ($res)  ref=$REFS/$name.png test=$OUT/$name.png"; fails=$((fails+1))
      fi
    else
      echo "NORENDER (see $OUT/$name.log)"; fails=$((fails+1))
    fi
  fi
done

echo "----"
echo "renderer: ${RENDERER:-(none reported)}"
echo "golden-render[$BACKEND]$([ "$UPDATE" = 1 ] && echo ' (update-refs)'): $passes ok, $fails failing, $skips skipped"
# In check mode, if there are no refs at all yet, SKIP (77) instead of failing —
# refs are generated once with --update-refs and committed alongside the branch.
if [ "$UPDATE" = 0 ] && [ "$passes" = 0 ] && [ "$fails" -gt 0 ] && ! ls "$REFS"/*.png >/dev/null 2>&1; then
  echo "SKIP: no references present — run --update-refs once and commit refs/$BACKEND/"; exit 77
fi
[ "$fails" = 0 ]
