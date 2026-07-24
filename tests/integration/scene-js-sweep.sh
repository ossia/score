#!/usr/bin/env bash
# Full-pipeline render-regression driven by the JS wiring system.
#
#   tests/integration/scene-js-sweep.sh [scripts-dir]   (run from the build root)
#
# Unlike csf-sweep.sh (which loads one shader into one process), this reuses the
# already-validated JS build scripts under csf-examples/.../tests-scene/scripts:
# each build-*.js calls common.js's autoBuilder(), which analyses the shader and
# assembles the *whole* documented pipeline — geometry producers, cameras,
# cubemaps, depth chains, raster/ModelDisplay consumers — then wires the result
# into a Window device. We run each script through the real ossia-score binary
# headless, autoplay it, and grab the Window's texture over OSC.
#
# Render path (proven on both GL backends):
#   SCORE_FORCE_OFFSCREEN_WINDOW=Window  -> Window device renders to an offscreen
#     window (no platform window is mapped; never touches the user's desktop).
#   QT_QPA_PLATFORM=offscreen            -> fully headless.
#   ossia-score --no-gui --no-restore --script build-X.js --wait 1 --autoplay
#   OSC /script Score.device('Window').grabTo(png)  on udp/6666, then /stop /exit
#
# Pass criterion = a non-blank PNG was produced. The process often exits with a
# SIGSEGV from GfxContext teardown *after* the grab has already been written, so
# the exit code is ignored and the PNG's pixel mean is the verdict.
set -u

SCRIPTS="${1:-$HOME/Documents/ossia/score/packages/csf-examples/csf-testers/tests-scene/scripts}"
BIN="${OSSIA_SCORE:-ossia-score}"
command -v "$BIN" >/dev/null 2>&1 || BIN="./ossia-score"
OUT_ROOT="${OUT_ROOT:-/tmp/scene-js-sweep}"
OSC_PORT=6666
GRAB_DELAY="${GRAB_DELAY:-6}"     # seconds to let the graph build + render before grabbing
BLANK_MEAN="${BLANK_MEAN:-0.002}" # pixel mean at/below which a PNG counts as blank

mapfile -t SCRIPTS_LIST < <(find "$SCRIPTS" -maxdepth 1 -type f -name 'build-*.js' | sort)
echo "Rendering ${#SCRIPTS_LIST[@]} JS pipelines from $SCRIPTS via $BIN"

# Backend GL-selecting env, set per pass by sweep_backend.
BACKEND_ENV=()

# One pipeline through one backend. Runs sequentially (single OSC control port).
run_one() { # js_path out_png
  local js="$1" png="$2"
  rm -f "$png"
  (
    sleep "$GRAB_DELAY"
    oscsend 127.0.0.1 "$OSC_PORT" /script s "Score.device('Window').grabTo('$png')"
    sleep 1.5; oscsend 127.0.0.1 "$OSC_PORT" /stop
    sleep 0.5; oscsend 127.0.0.1 "$OSC_PORT" /exit
  ) >/dev/null 2>&1 &
  local grabber=$!
  # SCORE_FORCE_OFFSCREEN_WINDOW renders to an offscreen surface (never maps a
  # window / captures the desktop). The GL platform/driver is chosen per backend
  # by BACKEND_ENV: llvmpipe uses offscreen-EGL software GL; nvidia uses the
  # xcb/GLX context on :0 (offscreen-EGL there tends to fall back to llvmpipe).
  # `-u DISPLAY` first: with DISPLAY=:0 in scope, offscreen-EGL + llvmpipe
  # negotiates a GL 2.0 context (too old for the RHI → everything fails); the
  # nvidia backend re-sets DISPLAY via BACKEND_ENV for its xcb/GLX context.
  env -u DISPLAY SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
      SCORE_FORCE_OFFSCREEN_WINDOW=Window \
      "${BACKEND_ENV[@]}" \
    timeout --foreground 30 "$BIN" --no-gui --no-restore \
      --script "$js" --wait 1 --autoplay >/dev/null 2>&1
  wait "$grabber" 2>/dev/null
}

verdict() { # png -> prints "PASS <mean>" / "BLANK <mean>" / "NORENDER"
  local png="$1"
  [ -s "$png" ] || { echo "NORENDER"; return; }
  local m; m=$(convert "$png" -format '%[fx:mean]' info: 2>/dev/null || echo 0)
  if awk "BEGIN{exit !($m > $BLANK_MEAN)}"; then echo "PASS $m"; else echo "BLANK $m"; fi
}

# backend name -> env prefix that selects the GL driver
sweep_backend() { # label env...
  local label="$1"; shift
  BACKEND_ENV=("$@")
  local outdir="$OUT_ROOT/$label"; mkdir -p "$outdir"
  echo "=== backend: $label ==="
  local pass=0 blank=0 norender=0
  for js in "${SCRIPTS_LIST[@]}"; do
    local name; name="$(basename "$js" .js)"
    local png="$outdir/$name.png"
    run_one "$js" "$png"
    local v; v="$(verdict "$png")"
    case "$v" in
      PASS*)     pass=$((pass+1));     printf '  %-42s ok    %s\n' "$name" "${v#PASS }" ;;
      BLANK*)    blank=$((blank+1));   printf '  %-42s BLANK %s\n' "$name" "${v#BLANK }" ;;
      NORENDER*) norender=$((norender+1)); printf '  %-42s NO-RENDER\n' "$name" ;;
    esac
  done
  echo "--- $label: $pass rendered, $blank blank, $norender no-render (of ${#SCRIPTS_LIST[@]}) ---"
}

# Two separate passes; llvmpipe first, then the machine's GPU. Select with
# BACKENDS="llvmpipe" / "nvidia" / "llvmpipe nvidia" (default: both).
for b in ${BACKENDS:-llvmpipe nvidia}; do
  case "$b" in
    llvmpipe) sweep_backend llvmpipe QT_QPA_PLATFORM=offscreen LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ;;
    nvidia)   sweep_backend nvidia   QT_QPA_PLATFORM=xcb DISPLAY="${DISPLAY:-:0}" __NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia ;;
    *) echo "unknown backend: $b" ;;
  esac
done

echo "PNGs under $OUT_ROOT/{llvmpipe,nvidia}/"
