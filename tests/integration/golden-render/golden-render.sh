#!/usr/bin/env bash
# Golden-image render regression harness.
#
#   golden-render.sh [--backend llvmpipe|nvidia|vulkan-lavapipe|nvidia-vulkan]
#                    [--update-refs] [--cases "name name ..."] [--keep-going]
#
# Renders each pinned tests-scene pipeline (cases-<backend-class>.txt, falling
# back to cases-llvmpipe.txt) through the real ossia-score binary headless —
# same proven recipe as /tmp/verify-shader.sh / scene-js-sweep.sh — and
# compares the grabbed frame against refs/<backend>/<case>.png with compare.py
# (SSIM/PSNR/max-abs, profile "strict").
#
#   check mode (default)  : ref must exist; verdict per case is
#                           PASS / FAIL / NOREF / NORENDER / SKIP-UNSTABLE.
#                           Exit 0 iff no FAIL/NOREF/NORENDER.
#   --update-refs         : renders each case TWICE and accepts the ref only if
#                           the two runs agree (compare.py --profile self) and
#                           are non-blank. Disagreeing cases are recorded in
#                           refs/<backend>/UNSTABLE.txt (time-animated shaders
#                           self-reject here); blank ones in BLANK.txt.
#
# Backend classes:
#   llvmpipe       offscreen QPA + software GL   (CI-able, fully headless)
#   nvidia         xcb on :0 + NVIDIA GLX        (rig)
#   vulkan-lavapipe/ nvidia-vulkan: GraphicsApi=Vulkan via an isolated
#   XDG_CONFIG_HOME (score reads score_plugin_gfx/GraphicsApi from QSettings;
#   there is no CLI flag). ALL runs use an isolated config home seeded from the
#   user's score.conf with GraphicsApi pinned — a run must not depend on the
#   user's live settings (they currently say Vulkan!) nor trip failsafe.bit.
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
while [ $# -gt 0 ]; do
  case "$1" in
    --backend) BACKEND="$2"; shift 2 ;;
    --update-refs) UPDATE=1; shift ;;
    --cases) CASES_OVERRIDE="$2"; shift 2 ;;
    --keep-going) KEEPGOING=1; shift ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

# Prerequisites -> ctest SKIP (return 77) rather than a hard failure.
command -v oscsend  >/dev/null || { echo "SKIP: oscsend not found";        exit 77; }
command -v convert  >/dev/null || { echo "SKIP: ImageMagick not found";     exit 77; }
[ -x "$BIN" ]                  || { echo "SKIP: $BIN not built";            exit 77; }
[ -d "$SCRIPTS" ]             || { echo "SKIP: corpus missing ($SCRIPTS)";  exit 77; }

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

# ---- isolated, pinned config home -------------------------------------------
# GraphicsApi comes from QSettings (score_plugin_gfx/GraphicsApi,
# Gfx/Settings/Model.cpp:38) — pin it so runs are hermetic.
CFG="$OUT/config-home"
mkdir -p "$CFG/ossia"
case "$BACKEND" in
  vulkan-*|*-vulkan) API=Vulkan ;;
  *) API=OpenGL ;;
esac
python3 - "$HOME/.config/ossia/score.conf" "$CFG/ossia/score.conf" "$API" <<'EOF'
import re, sys, pathlib
src, dst, api = sys.argv[1], sys.argv[2], sys.argv[3]
try:
    text = pathlib.Path(src).read_text()
except OSError:
    text = ""
if "[score_plugin_gfx]" not in text:
    text += "\n[score_plugin_gfx]\nGraphicsApi=%s\n" % api
elif re.search(r"^GraphicsApi=.*$", text, re.M):
    text = re.sub(r"^GraphicsApi=.*$", "GraphicsApi=%s" % api, text, flags=re.M)
else:
    text = text.replace("[score_plugin_gfx]", "[score_plugin_gfx]\nGraphicsApi=%s" % api)
pathlib.Path(dst).write_text(text)
EOF

# Backend env (mirrors scene-js-sweep.sh; `env -u DISPLAY` for llvmpipe because
# a live DISPLAY makes offscreen-EGL negotiate a GL 2.0 context).
backend_env() {
  case "$BACKEND" in
    nvidia|nvidia-vulkan)
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb __GLX_VENDOR_LIBRARY_NAME=nvidia" ;;
    vulkan-lavapipe)
      echo "DISPLAY=:0 QT_QPA_PLATFORM=xcb VK_LOADER_DRIVERS_SELECT=lvp*" ;;
    *)
      echo "QT_QPA_PLATFORM=offscreen LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe" ;;
  esac
}

pixel_mean() { convert "$1" -format '%[fx:mean]' info: 2>/dev/null || echo 0; }

# ---- one full app run -> one PNG --------------------------------------------
render_one() { # case_name out_png -> 0 ok, 2 no png
  local name="$1" png="$2" js="$SCRIPTS/$1.js" log="$OUT/$1.log"
  rm -f "$png"
  [ -f "$js" ] || { echo "  missing script $js" >&2; return 2; }
  local benv; benv=$(backend_env)
  (
    flock -w 300 9 || { echo "  LOCK-TIMEOUT" >&2; exit 4; }
    ( for _ in $(seq 1 "$GRABTRIES"); do
        sleep 2
        oscsend 127.0.0.1 $OSC /script s "Score.device('Window').grabTo('$png')" 2>/dev/null
        [ -s "$png" ] && break
      done
      sleep 0.5; oscsend 127.0.0.1 $OSC /stop; sleep 0.5; oscsend 127.0.0.1 $OSC /exit ) >/dev/null 2>&1 &
    # shellcheck disable=SC2086
    env -u DISPLAY XDG_CONFIG_HOME="$CFG" \
        SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
        SCORE_FORCE_OFFSCREEN_WINDOW=Window \
        ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/%p.profraw" \
        $benv \
      timeout --foreground "$TIMEOUT" "$BIN" --no-gui --no-restore \
        --script "$js" --wait 1 --autoplay >"$log" 2>&1
    wait 2>/dev/null
  ) 9>/tmp/score-harness.lock
  [ -s "$png" ] || return 2
  return 0
}

listed() { [ -f "$2" ] && grep -qx "$1" "$2"; }

fails=0; passes=0; skips=0
for name in "${CASES[@]}"; do
  printf '%-42s' "$name"
  if [ "$UPDATE" = 1 ]; then
    if render_one "$name" "$OUT/$name.A.png" && render_one "$name" "$OUT/$name.B.png"; then
      m=$(pixel_mean "$OUT/$name.A.png")
      if ! awk "BEGIN{exit !($m > $BLANK_MEAN)}"; then
        echo "BLANK mean=$m (not accepted as ref)"; grep -qx "$name" "$REFS/BLANK.txt" 2>/dev/null || echo "$name" >> "$REFS/BLANK.txt"
        skips=$((skips+1)); continue
      fi
      if res=$(python3 "$HERE/compare.py" "$OUT/$name.A.png" "$OUT/$name.B.png" --profile self); then
        cp "$OUT/$name.A.png" "$REFS/$name.png"
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
    if listed "$name" "$REFS/UNSTABLE.txt"; then echo "SKIP-UNSTABLE"; skips=$((skips+1)); continue; fi
    if listed "$name" "$REFS/BLANK.txt"; then echo "SKIP-BLANK"; skips=$((skips+1)); continue; fi
    if [ ! -f "$REFS/$name.png" ]; then echo "NOREF (run --update-refs)"; fails=$((fails+1)); continue; fi
    if render_one "$name" "$OUT/$name.png"; then
      if res=$(python3 "$HERE/compare.py" "$REFS/$name.png" "$OUT/$name.png" --profile strict); then
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
echo "golden-render[$BACKEND]$([ "$UPDATE" = 1 ] && echo ' (update-refs)'): $passes ok, $fails failing, $skips skipped"
# In check mode, if there are no refs at all yet, SKIP (77) instead of failing —
# refs are generated once with --update-refs and committed alongside the branch.
if [ "$UPDATE" = 0 ] && [ "$passes" = 0 ] && [ "$fails" -gt 0 ] && ! ls "$REFS"/*.png >/dev/null 2>&1; then
  echo "SKIP: no references present — run --update-refs once and commit refs/$BACKEND/"; exit 77
fi
[ "$fails" = 0 ]
