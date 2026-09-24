#!/usr/bin/env bash
# Multi-outlet texture validation.
#
#   tests/integration/multi-outlet/multi-outlet.sh
#
# A headless llvmpipe app run plays multi-outlet.js: an Images process feeds an
# Image Processor (Depth Anything 3 metric: output 0 depth, output 1 sky), and
# a "Grid 2x2" ISF tiles the source and the processor's Image / Mask / Depth
# outlets onto Window:/ (the ISF gets its program from Grid 2x2.scp, while
# playing in the "live" run). Frames
# are grabbed over OSC /script until the model has run, and analyze.py asserts
# that each outlet shows its own texture (see analyze.py).
#
# It runs twice: with the processor -> grid cables in the document from the
# start ("init"), and with them -- and the Grid preset -- applied over OSC while
# playing ("live"), since those two cases go through different code paths.
#
# Environment:
#   OSSIA_SCORE          binary (default: build-developer/ossia-score)
#   MULTI_OUTLET_IMAGE   test photo with sky (default: ailia depth_anything demo1.png)
#   MULTI_OUTLET_MODEL   Depth Anything 3 metric ONNX, inputs 1x3x280x504, outputs depth + sky
#   OUT                  output directory (default: /tmp/multi-outlet)
#
# The render environment is the same as text-render.sh: our own Xvfb, xcb,
# Mesa llvmpipe forced. ONNX Runtime is pinned to the CPU provider.
#
# PASS = both runs exit 0, no JS SCENARIO-ERROR, analyze.py green on both.
# Self-serializes on flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
SRCROOT="$(cd "$HERE/../../.." && pwd)"
BIN="${OSSIA_SCORE:-$SRCROOT/build-developer/ossia-score}"
OUT="${OUT:-/tmp/multi-outlet}"
IMAGE="${MULTI_OUTLET_IMAGE:-$HOME/projets/oss/ailia-models/depth_estimation/depth_anything/demo1.png}"
MODEL="${MULTI_OUTLET_MODEL:-/mnt/win2/models/models-presets/models/image-processor/depth-anything-v3-metric-large-280x504.onnx}"
OSC=6666
TIMEOUT="${TIMEOUT:-420}"
GRABS="${GRABS:-40}"

command -v oscsend >/dev/null || { echo "SKIP: oscsend not found"; exit 77; }
[ -x "$BIN" ] || { echo "SKIP: $BIN not built"; exit 77; }
[ -f "$IMAGE" ] || { echo "SKIP: test image $IMAGE missing"; exit 77; }
[ -f "$MODEL" ] || { echo "SKIP: model $MODEL missing"; exit 77; }
python3 -c "import numpy, PIL, onnxruntime" 2>/dev/null \
  || { echo "SKIP: python numpy/PIL/onnxruntime missing"; exit 77; }

rm -rf "$OUT"; mkdir -p "$OUT"

# ---- a real X server, ours if we can have one -------------------------------
XVFB_PID=""
DISP=""
if command -v Xvfb >/dev/null && command -v xdpyinfo >/dev/null; then
  for n in $(seq 90 120); do
    [ -e "/tmp/.X11-unix/X$n" ] && continue
    Xvfb ":$n" -screen 0 1920x1080x24 +extension GLX >"$OUT/xvfb.log" 2>&1 &
    XVFB_PID=$!
    for _ in $(seq 1 20); do
      DISPLAY=":$n" xdpyinfo >/dev/null 2>&1 && { DISP=":$n"; break; }
      kill -0 "$XVFB_PID" 2>/dev/null || break
      sleep 0.25
    done
    [ -n "$DISP" ] && break
    kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null; XVFB_PID=""
  done
fi
if [ -z "$DISP" ] && [ -n "${DISPLAY:-}" ]; then DISP="$DISPLAY"; fi
[ -n "$DISP" ] || { echo "SKIP: no X display"; exit 77; }
cleanup_xvfb() { [ -n "$XVFB_PID" ] && { kill "$XVFB_PID" 2>/dev/null; wait "$XVFB_PID" 2>/dev/null; }; }
trap cleanup_xvfb EXIT
echo "display=$DISP$([ -n "$XVFB_PID" ] && echo ' (own Xvfb)' || echo ' (inherited)')"

# Hermetic config home, GraphicsApi pinned to OpenGL (user conf may say Vulkan).
CFG="$OUT/config-home"; mkdir -p "$CFG/ossia"
python3 - "${XDG_CONFIG_HOME:-$HOME/.config}/ossia/score.conf" "$CFG/ossia/score.conf" <<'PYEOF'
import re, sys, pathlib
src, dst = sys.argv[1], sys.argv[2]
try: text = pathlib.Path(src).read_text()
except OSError: text = ""
if "[score_plugin_gfx]" not in text:
    text += "\n[score_plugin_gfx]\nGraphicsApi=OpenGL\n"
elif re.search(r"^GraphicsApi=.*$", text, re.M):
    text = re.sub(r"^GraphicsApi=.*$", "GraphicsApi=OpenGL", text, flags=re.M)
else:
    text = text.replace("[score_plugin_gfx]", "[score_plugin_gfx]\nGraphicsApi=OpenGL")
pathlib.Path(dst).write_text(text)
PYEOF

jsstr() { python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$1"; }

send() { oscsend 127.0.0.1 $OSC "$@" 2>/dev/null; }

grab() { # png -> 0 iff file written
  local png="$1"
  rm -f "$png"
  for _ in $(seq 1 12); do
    send /script s "Score.device('Window').grabTo('$png')"
    sleep 0.6; [ -s "$png" ] && return 0
  done
  return 1
}

# The model runs on the worker thread: grab until the Mask tile has content.
mask_ready() {
  python3 - "$1" <<'PYEOF'
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(np.float32) / 255
h, w, _ = a.shape
bl = a[h // 2 + h // 20: h - h // 20, w // 20: w // 2 - w // 20, 0]
sys.exit(0 if bl.std() > 0.05 else 1)
PYEOF
}

# run_mode <init|live>: one app run, grabs to $OUT/<mode>/grid.png.
run_mode() {
  local mode="$1" dir="$OUT/$1"
  mkdir -p "$dir"
  {
    printf 'var OUT_DIR = %s;\n' "$(jsstr "$dir")"
    printf 'var IMAGE = %s;\n' "$(jsstr "$IMAGE")"
    printf 'var MODEL = %s;\n' "$(jsstr "$MODEL")"
    printf 'var GRID_PRESET = '; cat "$HERE/Grid 2x2.scp"; printf ';\n'
    printf 'var WIRING = %s;\n' "$(jsstr "$mode")"
    cat "$HERE/multi-outlet.js"
  } > "$dir/scene.js"

  rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/ossia/failsafe.bit"
  (
    flock -w 900 9 || { echo 98 > "$dir/run.rc"; exit 0; }
    # A previous run still shutting down can hold the OSC port: the new app then
    # fails to listen ("asio listen error"), never gets /script, and the run
    # times out. Wait for the port and for any earlier instance of this test.
    for _ in $(seq 1 60); do
      pgrep -f -- "--script $OUT/" >/dev/null 2>&1 && { sleep 1; continue; }
      ss -Hlun "sport = :$OSC" 2>/dev/null | grep -q . && { sleep 1; continue; }
      break
    done
    env -u DISPLAY XDG_CONFIG_HOME="$CFG" \
        SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 SCORE_ONNX_FORCE_PROVIDER=cpu \
        SCORE_FORCE_OFFSCREEN_WINDOW=Window \
        DISPLAY="$DISP" QT_QPA_PLATFORM=xcb \
        __GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
        QT_LOGGING_RULES='qt.rhi.general=true' \
      timeout --foreground "$TIMEOUT" "$BIN" --no-gui --no-restore \
        --script "$dir/scene.js" --wait 1 --autoplay >"$dir/run.log" 2>&1 &
    local APP=$!

    local ok=0
    for _ in $(seq 1 120); do [ -s "$dir/multi-outlet-init.score" ] && { ok=1; break; }; sleep 1; done
    if [ "$ok" = 0 ]; then
      echo "[$mode] no readiness marker -- startup failed (see $dir/run.log)" >&2
      kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$dir/run.rc"; exit 0
    fi
    sleep 3
    if [ "$mode" = live ]; then
      # Playback runs and the graph is built: add the processor -> grid cables now.
      send /script s "wireOutlets()"
      sleep 2
    fi

    local i
    for i in $(seq 1 "$GRABS"); do
      grab "$dir/grid.png" || { echo "GRAB-FAIL $i" >> "$dir/run.log"; continue; }
      if mask_ready "$dir/grid.png"; then
        sleep 2; grab "$dir/grid.png" # one more frame, so Depth has landed too
        echo "[$mode] grid ready after $i grabs"
        break
      fi
      sleep 2
    done

    send /script s "finalizeRun()"
    sleep 1
    send /stop; sleep 0.5
    send /exit s force
    wait "$APP"; echo $? > "$dir/run.rc"
  ) 9>/tmp/score-harness.lock
}

check_mode() { # mode -> appends to $FAILS
  local mode="$1" dir="$OUT/$1" rc renderer
  echo "=== $mode wiring"
  rc=$(cat "$dir/run.rc" 2>/dev/null || echo 97)
  [ "$rc" = 0 ] || FAILS+=" $mode:exit=$rc"
  if grep -q "NULL RHI BACKEND" "$dir/run.log" 2>/dev/null; then
    FAILS+=" $mode:NULL-RHI"
  else
    # Some builds never print the qt.rhi.general line (text-render has the same
    # NO-RENDERER-LINE): only a renderer that IS reported and is not llvmpipe
    # fails. The Null backend is caught above, and would also fail analyze.py
    # (it fills every texture with yellow).
    renderer=$(grep -m1 'qt\.rhi\.general: OpenGL VENDOR' "$dir/run.log" 2>/dev/null | sed 's/^.*qt\.rhi\.general: //')
    echo "backend: ${renderer:-not reported}"
    if [ -n "$renderer" ] && ! printf '%s' "$renderer" | grep -q llvmpipe; then
      FAILS+=" $mode:WRONG-BACKEND"
    fi
  fi
  grep -q "SCENARIO-ERROR" "$dir/run.log" 2>/dev/null && FAILS+=" $mode:JSERR"
  if [ -s "$dir/grid.png" ]; then
    python3 "$HERE/analyze.py" "$dir/grid.png" "$IMAGE" "$MODEL" || FAILS+=" $mode:ANALYZE"
  else
    FAILS+=" $mode:NO-GRAB"
  fi
}

FAILS=""
for mode in init live; do
  run_mode "$mode"
  check_mode "$mode"
done

if [ -z "$FAILS" ]; then
  echo "multi-outlet PASS (grabs: $OUT/init/grid.png $OUT/live/grid.png)"
else
  echo "multi-outlet FAIL:$FAILS  (out=$OUT)"; exit 1
fi
