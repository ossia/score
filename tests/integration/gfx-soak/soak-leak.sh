#!/usr/bin/env bash
# gfx-soak — resource/leak/lifetime soak runner.
#
#   tests/integration/gfx-soak/soak-leak.sh [ncycles]
#
# Plays soak.js headless on llvmpipe (ASAN build) and pumps `cycle()` over
# OSC /script — each cycle creates + wires + destroys K=4 gfx processes while
# the graph renders — sampling VmRSS / open-fd count from /proc/<pid> as it
# goes. See soak.js for what one cycle exercises.
#
# PASS requires ALL of:
#   1. exit code 0 (teardown survives a long churn session)
#   2. zero "ERROR: AddressSanitizer" in the log
#   3. zero CYCLE-ERROR / TEARDOWN-ERROR (mutations really happened)
#   4. >= 90% of pumped cycles executed
#   5. final grab non-blank (the pipeline still renders after the churn)
#   6. gfx-process population in final.score == baseline init.score
#   7. post-warmup RSS growth < SLOPE_KB_PER_CYCLE (linear fit; catches
#      unbounded growth without exact counts under ASAN's noisy allocator)
#   8. open-fd count stable (last - first <= FD_SLACK)
#
# Runs under flock /tmp/score-harness.lock (OSC port 6666 is global).
set -u

SRCROOT="$(cd "$(dirname "$0")/../../.." && pwd)"  # tests/integration/gfx-soak -> repo root
BIN="${OSSIA_SCORE:-$SRCROOT/build-sanitizers/ossia-score}"
SOAK_DIR="$(cd "$(dirname "$0")" && pwd)"
JS="$SOAK_DIR/soak.js"
OUT="${OUT:-/tmp/gfx-soak}"
OSC=6666
N="${1:-250}"
TICK="${TICK:-0.25}"
SAMPLE_EVERY="${SAMPLE_EVERY:-20}"
SLOPE_KB_PER_CYCLE="${SLOPE_KB_PER_CYCLE:-40}"
FD_SLACK="${FD_SLACK:-8}"
BLANK_MEAN="${BLANK_MEAN:-0.002}"
# quarantine capped + periodic release-to-OS so RSS tracks live heap, not
# ASAN bookkeeping noise; leaks are asserted via the slope, not LSAN (Qt/GL
# driver noise).
ASAN="detect_leaks=0:halt_on_error=0:handle_segv=1:detect_odr_violation=0:protect_shadow_gap=0:quarantine_size_mb=16:allocator_release_to_os_interval_ms=2000"

# Prerequisites -> ctest SKIP (77).
command -v oscsend >/dev/null || { echo "SKIP: oscsend not found";    exit 77; }
command -v convert >/dev/null || { echo "SKIP: ImageMagick not found"; exit 77; }
[ -x "$BIN" ]                 || { echo "SKIP: $BIN not built";        exit 77; }
[ -f "$JS" ]                  || { echo "SKIP: soak.js missing";       exit 77; }

if [ -z "${DISPLAY:-}" ]; then
  for d in 99 98 97; do
    if command -v Xvfb >/dev/null 2>&1; then Xvfb ":$d" -screen 0 1280x720x24 >/dev/null 2>&1 &
    elif command -v Xephyr >/dev/null 2>&1; then Xephyr ":$d" -screen 1280x720 -ac -noreset >/dev/null 2>&1 &
    else break; fi
    OWN_X=$!; sleep 3
    if DISPLAY=":$d" xdpyinfo >/dev/null 2>&1; then
      export DISPLAY=":$d"; trap 'kill "$OWN_X" 2>/dev/null' EXIT; break
    fi
    kill "$OWN_X" 2>/dev/null
  done
fi
if [ -z "${DISPLAY:-}" ]; then
  echo "gfx-soak: no X server; offscreen is not a fallback (no GL -> Null"
  echo "  backend -> the non-blank check would pass on a constant). SKIP."
  exit 77
fi

mkdir -p "$OUT"
rm -f "$OUT"/init.score "$OUT"/final.score "$OUT"/final.png "$OUT"/soak.log \
      "$OUT"/samples.csv "$OUT"/soak.rc "$HOME/.config/ossia/failsafe.bit"

# Hermetic config home: pin GraphicsApi=OpenGL (the user's live score.conf may
# say Vulkan; QSettings is the only way score picks the API).
CFG="$OUT/config-home"; mkdir -p "$CFG/ossia"
python3 - "$HOME/.config/ossia/score.conf" "$CFG/ossia/score.conf" <<'EOF'
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
EOF

send() { oscsend 127.0.0.1 $OSC "$@" 2>/dev/null; }

sample() { # cycle pid
  local st="/proc/$2/status"
  [ -r "$st" ] || return 0
  local rss fds
  rss=$(awk '/^VmRSS:/{print $2}' "$st")
  fds=$(ls "/proc/$2/fd" 2>/dev/null | wc -l)
  echo "$1,$rss,$fds" >> "$OUT/samples.csv"
}

echo "gfx-soak: $N cycles x K=4 procs, tick=${TICK}s, bin=$BIN"
echo "cycle,rss_kb,fds" > "$OUT/samples.csv"

(
  flock -w 900 9 || { echo 98 > "$OUT/soak.rc"; exit 0; }
  # Stage with the script's own directory injected: Score.readFile resolves
  # nothing relative to the running script, so soak.js takes its corpus from
  # SOAK_DIR rather than an absolute path.
  { printf 'var SOAK_DIR = "%s";\n' "$SOAK_DIR"; cat "$JS"; } > "$OUT/soak.staged.js"
  # Real X + xcb + a real window. Under QT_QPA_PLATFORM=offscreen there is no
  # GL (Qt's offscreen integration is GLX-only), QRhi falls back to the Null
  # backend and renders nothing, so criterion 5 -- "final grab non-blank" --
  # passed on a constant colour and verified nothing. The RSS measurement was
  # unaffected, but the render check was not.
  env XDG_CONFIG_HOME="$CFG" \
      SCORE_AUDIO_BACKEND=dummy SCORE_DISABLE_AUDIOPLUGINS=1 \
      SCORE_SANITIZE_SKIP_CHECKS=1 QT_QPA_PLATFORM=xcb \
      LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
      ASAN_OPTIONS="$ASAN" LLVM_PROFILE_FILE="$OUT/soak.profraw" \
    timeout --foreground 900 "$BIN" --no-restore \
      --script "$OUT/soak.staged.js" --wait 1 --autoplay >"$OUT/soak.log" 2>&1 &
  APP=$!

  # wait for the readiness marker (ASAN startup is slow), let play begin
  for _ in $(seq 1 120); do [ -s "$OUT/init.score" ] && break; sleep 1; done
  if [ ! -s "$OUT/init.score" ]; then
    echo "no readiness marker — startup failed (see $OUT/soak.log)" >&2
    kill "$APP" 2>/dev/null; wait "$APP" 2>/dev/null; echo 97 > "$OUT/soak.rc"; exit 0
  fi
  sleep 3
  # $APP is the `timeout` wrapper (RSS ~2MB); resolve the real ossia-score
  # child so RSS/fd sampling measures the engine, not the wrapper.
  SPID=$(pgrep -P "$APP" -f ossia-score | head -1)
  [ -n "$SPID" ] || SPID=$(pgrep -f "ossia-score --no-gui --no-restore --script $OUT/soak.staged.js" | head -1)
  [ -n "$SPID" ] || SPID="$APP"
  sample 0 "$SPID"

  for i in $(seq 1 "$N"); do
    send /script s "cycle()"
    sleep "$TICK"
    [ $((i % SAMPLE_EVERY)) = 0 ] && sample "$i" "$SPID"
    kill -0 "$APP" 2>/dev/null || break   # app died mid-storm: stop pumping
  done

  # settle, dump final doc, grab proof-of-life frame, exit
  sleep 2; sample "final" "$SPID"
  send /script s "teardown()"; sleep 1
  for _ in $(seq 1 15); do
    send /script s "Score.device('Window').grabTo('$OUT/final.png')"
    sleep 1; [ -s "$OUT/final.png" ] && break
  done
  # /script, not bare /stop and /exit: this oscsend emits argument-less messages
  # that score's OSC listener rejects, so shutdown was never delivered.
  send /script s "Score.stop()"; sleep 0.5
  send /exit s force
  wait "$APP"; echo $? > "$OUT/soak.rc"
) 9>/tmp/score-harness.lock

# ---------------- verdict ----------------
python3 - "$OUT" "$N" "$SLOPE_KB_PER_CYCLE" "$FD_SLACK" "$BLANK_MEAN" <<'EOF'
import re, subprocess, sys
out, n, slope_max, fd_slack, blank = sys.argv[1], int(sys.argv[2]), float(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])
bad = []

rc = open(f"{out}/soak.rc").read().strip() if True else "?"
if rc != "0": bad.append(f"exit={rc}")

log = open(f"{out}/soak.log", errors="replace").read()
if "ERROR: AddressSanitizer" in log: bad.append("ASAN")
nerr = log.count("CYCLE-ERROR") + log.count("TEARDOWN-ERROR") + log.count("INIT-ERROR")
if nerr: bad.append(f"jserrors={nerr}")
done = len(re.findall(r"\[gfx-soak\] cycle \d+ done", log))
if done < 0.9 * n: bad.append(f"cycles={done}/{n}")

try:
    mean = float(subprocess.check_output(
        ["convert", f"{out}/final.png", "-format", "%[fx:mean]", "info:"]).decode())
    if mean <= blank: bad.append(f"BLANK mean={mean}")
except Exception as e:
    bad.append(f"NORENDER ({e.__class__.__name__})")
    mean = -1

def count_procs(path):
    try: return open(path, errors="replace").read().count("74ca45ff-92c9-44a0-8f1a-754dea05ee1b")
    except OSError: return -1
pi, pf = count_procs(f"{out}/init.score"), count_procs(f"{out}/final.score")
if pi < 0 or pf < 0 or pi != pf: bad.append(f"proc-count init={pi} final={pf}")

rows = [l.split(",") for l in open(f"{out}/samples.csv").read().splitlines()[1:]]
num = [(int(c), int(r), int(f)) for c, r, f in rows if c.isdigit()]
slope = None
if len(num) >= 4:
    post = num[max(1, len(num)//4):]          # discard warmup quarter
    xs = [c for c, _, _ in post]; ys = [r for _, r, _ in post]
    mx, my = sum(xs)/len(xs), sum(ys)/len(ys)
    den = sum((x-mx)**2 for x in xs)
    slope = sum((x-mx)*(y-my) for x, y in zip(xs, ys))/den if den else 0.0
    if slope > slope_max: bad.append(f"rss-slope={slope:.1f}KB/cycle>{slope_max}")
    fds = [f for _, _, f in num]
    if fds[-1] - fds[0] > fd_slack: bad.append(f"fd-growth={fds[0]}->{fds[-1]}")
else:
    bad.append(f"too-few-samples={len(num)}")

srng = f"{num[0][1]//1024}->{num[-1][1]//1024}MB" if num else "?"
info = f"cycles={done}/{n} rss={srng} slope={'%.1f' % slope if slope is not None else '?'}KB/c mean={mean:.4f} procs={pi}->{pf}"
if bad:
    print(f"gfx-soak FAIL: {' '.join(bad)}  ({info})  log={out}/soak.log"); sys.exit(1)
print(f"gfx-soak PASS  ({info})")
EOF
