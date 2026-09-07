#!/usr/bin/env bash
# =============================================================================
# GPU VALIDATION MATRIX — every backend against every driver, goggles on.
#
# Runs the gfx+threedim suite once per (backend, driver) cell with every
# validation layer the platform offers, and reports BOTH the test verdict and
# the validation-message census: distinct VUIDs, sync hazards, ASan and UBSan
# hits. Intended to be started on an IDLE machine and left alone.
#
#   ./gpu-validation-matrix.sh                     # auto-detect, default scope
#   BUILD=/path/to/build ./gpu-validation-matrix.sh
#   CELLS="vk-nvidia gl-llvmpipe" ./gpu-validation-matrix.sh
#   SUBSET=heavy ./gpu-validation-matrix.sh        # ~30 GPU-heavy tests/cell
#   SUBSET=all   ./gpu-validation-matrix.sh        # everything (SLOW, see below)
#   BATCH=1      ./gpu-validation-matrix.sh        # one ctest run for the cell
#   PRUNE=0      ./gpu-validation-matrix.sh        # keep each test's artefacts
#
# DEFAULT MODE IS ONE TEST AT A TIME: each test is BUILT, then RUN, then the
# next. That matters on the debug-SDK configuration, where a full build is
# ~1.1 TB and machines have ~400 GB: building everything up front simply does
# not fit. Building per test also means a crash or an out-of-disk stops after
# one target instead of losing the whole cell, and every test gets its own
# log, its own device banner and its own validation census. BATCH=1 restores
# the old behaviour (build nothing, one ctest invocation per cell) when the
# build already exists and you want the cell to finish faster.
#
# PRUNE (default on in per-test mode) deletes each test's executable and its
# object directory as soon as it has run. That is what keeps the working set
# flat instead of growing to the size of the whole suite: on a -O0 -g3 build
# with static sanitizer runtimes a single test binary is hundreds of MB, and
# 163 of them is what makes the reference debug tree 1.1 TB. Shared libraries
# and plugins are NEVER pruned -- every test needs them and rebuilding them
# per test would dominate the runtime.
#
# ---------------------------------------------------------------------------
# WHY THIS SCRIPT EXISTS RATHER THAN A ctest INVOCATION
#
# Four things silently produce confident, worthless numbers here. Each is
# handled below and each was measured, not guessed:
#
#  1. A cell can run on the WRONG DEVICE. `LIBGL_ALWAYS_SOFTWARE=1` alone does
#     not give llvmpipe when libglvnd is present — it is silently ignored and
#     you measure the discrete GPU. On a hybrid laptop the default GL renderer
#     is the iGPU, so a "GL/NVIDIA" cell measures Intel unless PRIME offload is
#     requested. EVERY cell here asserts its actual renderer before running.
#  2. A cell can run with NO VALIDATION LAYER and report zero errors for the
#     wrong reason. Vulkan cells assert the layer is resident.
#  3. A backend can silently fall back to QRhi::Null and pass vacuously. The
#     device banner is captured per cell.
#  4. ctest's exit code through a pipe is the LAST command's. Never trust
#     "0 failures" without checking how many tests actually ran.
#
# ---------------------------------------------------------------------------
# COST — measure before you budget
#
# Per-test cost depends enormously on the build:
#   Release / RelWithDebInfo ........  ~5-30 s
#   Debug + ASan + UBSan + validation  ~193 s   (measured, lenovo-cachyos)
#
# So a 184-test cell is ~25 min on a release build and ~10 HOURS on the debug
# one. SUBSET=heavy exists for that reason. Timeouts below are derived from the
# measured cost, not assumed.
# =============================================================================
set -u

BUILD="${BUILD:-$PWD}"
OUT="${OUT:-/tmp/gpu-validation-matrix}"
SUBSET="${SUBSET:-heavy}"
SECONDS_PER_TEST="${SECONDS_PER_TEST:-}"
BATCH="${BATCH:-0}"                 # 1 = one ctest run per cell (no building)
BUILD_JOBS="${BUILD_JOBS:-4}"       # ninja parallelism; 62 GB/12 cores OOMs above ~4
MIN_FREE_GB="${MIN_FREE_GB:-40}"    # stop rather than fill the filesystem
TEST_TIMEOUT="${TEST_TIMEOUT:-900}" # per individual test
PRUNE="${PRUNE:-1}"                 # delete each test's artefacts after running it

# ---- scope ------------------------------------------------------------------
EXCL='soak|object_csf_sweep|object_render_sweep|shader_sweep|golden_render|live_edit'
case "$SUBSET" in
  all)   SCOPE='gfx|threedim' ;;
  # A representative spread: indirect-draw ladder, CSF/compute, storage buffers,
  # render targets, multiview, geometry, scene, and the window/output paths.
  heavy) SCOPE='indirect|csf|storage|render_target|multiview|geometry|scene|cubemap|instanc|buffer|mesh|window_output|screen_output' ;;
  *)     SCOPE="$SUBSET" ;;
esac

# ---- environment traps ------------------------------------------------------
# The debug SDK ships an ASan-instrumented clang; LeakSanitizer then fires at
# COMPILER exit and ninja reads it as a failed compile. Tests need it too.
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
export LSAN_OPTIONS="${LSAN_OPTIONS:-detect_leaks=0}"
# A Debug build (-UNDEBUG) forces gpuDebugRequested()==true. A Release build
# needs this; =2 additionally enables D3D12 GPU-Based Validation.
export SCORE_GPU_VALIDATION="${SCORE_GPU_VALIDATION:-1}"

# X11 only exists on Linux; macOS and Windows use their native QPA plugin and
# must NOT have QT_QPA_PLATFORM forced to xcb.
if [ "$(uname -s)" = Linux ]; then
  if [ -z "${DISPLAY:-}" ]; then export DISPLAY=:0; fi
  if [ -z "${XAUTHORITY:-}" ]; then
    for x in "$HOME/.Xauthority" /run/user/$(id -u)/lyxauth /run/user/$(id -u)/gdm/Xauthority; do
      [ -f "$x" ] && { export XAUTHORITY="$x"; break; }
    done
  fi
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
fi

# Vulkan goggles: core validation + synchronization + best practices.
VKVAL='VK_LOADER_LAYERS_ENABLE=*validation* VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT'

# ---- platform ---------------------------------------------------------------
case "$(uname -s)" in
  Darwin)             OS=mac ;;
  MINGW*|MSYS*|CYGWIN*) OS=win ;;
  *)                  OS=linux ;;
esac

# ---- cell definitions -------------------------------------------------------
# label | SCORE_TEST_API | env | expected device regex
#
# The driver-selection variables are Mesa/loader specific and only meaningful
# on Linux. macOS has exactly one Metal device and no GLX; Windows selects the
# API rather than the driver, and its D3D validation is a score-level switch
# (SCORE_GPU_VALIDATION=2 additionally turns on D3D12 GPU-Based Validation
# before QRhi creates the device -- ScreenNode.cpp:84).
cell_def() {
  case "$1" in
    # -- Linux ---------------------------------------------------------------
    vk-nvidia)   echo "vulkan|VK_LOADER_DRIVERS_SELECT=nvidia*|NVIDIA|GeForce|Quadro|RTX" ;;
    vk-intel)    echo "vulkan|VK_LOADER_DRIVERS_SELECT=intel*|Intel|ARL|Arc|Iris" ;;
    vk-lavapipe) echo "vulkan|VK_LOADER_DRIVERS_SELECT=lvp*|llvmpipe|lavapipe" ;;
    vk-amd)      echo "vulkan|VK_LOADER_DRIVERS_SELECT=radeon*|AMD|RADV" ;;
    gl-nvidia)   echo "opengl|__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia|NVIDIA|GeForce|Quadro|RTX" ;;
    gl-intel)    echo "opengl|__GLX_VENDOR_LIBRARY_NAME=mesa MESA_LOADER_DRIVER_OVERRIDE=iris|Intel|ARL|Arc|Iris" ;;
    gl-llvmpipe) echo "opengl|__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe|llvmpipe" ;;
    # -- macOS ---------------------------------------------------------------
    # METAL_DEVICE_WRAPPER_TYPE=1 is Metal's API validation layer.
    metal)       echo "metal|METAL_DEVICE_WRAPPER_TYPE=1|Apple|AMD|Intel|Metal" ;;
    mac-opengl)  echo "opengl||Apple|AMD|Intel|ATI" ;;
    # -- Windows -------------------------------------------------------------
    d3d11)       echo "d3d11|SCORE_GPU_VALIDATION=2|." ;;
    d3d12)       echo "d3d12|SCORE_GPU_VALIDATION=2|." ;;
    win-vulkan)  echo "vulkan|SCORE_GPU_VALIDATION=1|." ;;
    win-opengl)  echo "opengl|SCORE_GPU_VALIDATION=1|." ;;
    *) echo "" ;;
  esac
}
case "$OS" in
  mac)   ALL_CELLS="metal mac-opengl" ;;
  win)   ALL_CELLS="d3d11 d3d12 win-vulkan win-opengl" ;;
  *)     ALL_CELLS="vk-nvidia vk-intel vk-lavapipe vk-amd gl-nvidia gl-intel gl-llvmpipe" ;;
esac
CELLS="${CELLS:-$ALL_CELLS}"

# macOS has no timeout(1) -- it is gtimeout from coreutils, and only if
# installed. Without this the cell command dies instantly with
# "env: timeout: No such file or directory" and reports an empty log.
if command -v timeout >/dev/null 2>&1;      then TMO="timeout"
elif command -v gtimeout >/dev/null 2>&1;   then TMO="gtimeout"
else TMO=""; echo "-- note: no timeout(1)/gtimeout(1); cells run unbounded"; fi

mkdir -p "$OUT"
cd "$BUILD" || { echo "!! BUILD=$BUILD is not a directory"; exit 1; }
[ -f CTestTestfile.cmake ] || { echo "!! $BUILD is not a CMake build dir"; exit 1; }

NTESTS=$(ctest -R "$SCOPE" -E "$EXCL" -N 2>/dev/null | sed -n 's/^Total Tests: //p' | tr -d " ")
[ -z "$NTESTS" ] && NTESTS=0
if [ "$NTESTS" -eq 0 ]; then echo "!! no tests match scope '$SCOPE'"; exit 1; fi

# Budget from a measured probe unless told otherwise: run one cheap test and
# scale, floored at 30 s/test so a fast probe cannot under-budget a slow suite.
if [ -z "$SECONDS_PER_TEST" ]; then
  echo "-- probing per-test cost..."
  t0=$SECONDS
  ctest -R "$SCOPE" -E "$EXCL" -N >/dev/null 2>&1
  ctest -R "$SCOPE" -E "$EXCL" -I 1,1 >/dev/null 2>&1
  probe=$(( SECONDS - t0 )); [ "$probe" -lt 30 ] && probe=30
  SECONDS_PER_TEST=$probe
fi
CELL_TIMEOUT=$(( NTESTS * SECONDS_PER_TEST * 2 ))
[ "$CELL_TIMEOUT" -lt 1800 ] && CELL_TIMEOUT=1800

echo "==============================================================="
echo " build            : $BUILD"
echo " scope            : $SUBSET  ($NTESTS tests)"
echo " per-test budget  : ${SECONDS_PER_TEST}s  -> cell timeout $((CELL_TIMEOUT/60)) min"
echo " results          : $OUT"
echo "==============================================================="

# Pre-flight device probe. vulkaninfo / glxinfo only exist on Linux, so this
# returns empty elsewhere and the cell relies on the post-run banner instead
# (see device_from_log, which is the authoritative check on every platform).
# Free space on the build filesystem, in GB. A debug build can consume
# hundreds of GB; filling / on a shared machine is a genuinely damaging
# outcome, so every per-test build checks this first and stops cleanly.
free_gb() { df -Pk "$BUILD" 2>/dev/null | awk 'NR==2 {print int($4/1048576)}'; }

# ctest name -> ninja target. The build lays tests out as tests/<dir>/<name>,
# so ask ninja rather than guessing the directory.
target_for() {
  ninja -C "$BUILD" -t targets all 2>/dev/null \
    | sed -n "s#^\(tests/[a-z0-9_]*/$1\):.*#\1#p" | head -1
}

renderer_of() { # $1 = api, $2 = env
  case "$1" in
    vulkan) command -v vulkaninfo >/dev/null 2>&1 || return 0
            env $2 vulkaninfo --summary 2>/dev/null | grep -m1 "deviceName" | sed 's/.*= //' ;;
    opengl) command -v glxinfo >/dev/null 2>&1 || return 0
            env $2 glxinfo -B 2>/dev/null | grep -m1 "OpenGL renderer" | sed 's/.*: //' ;;
    *)      return 0 ;;
  esac
}

# THE portable positive control: score prints its own RHI device banner
#   score.gfx: RHI device: backend=Vulkan device="NVIDIA GeForce RTX 4090" ...
# into every test log, on every platform (RenderList.cpp:1682). It answers both
# questions that matter -- which device did we really get, and did the backend
# silently fall back to QRhi::Null and pass vacuously.
device_from_log() { grep -ohE 'RHI device: backend=[^ ]+ device="[^"]*"' "$1" 2>/dev/null | sort -u | head -1; }
backend_from_log() { device_from_log "$1" | sed -E 's/.*backend=([^ ]+).*/\1/'; }

printf "\n%-14s %-34s %s\n" "CELL" "DEVICE (positive control)" "VERDICT"
printf -- "---------------------------------------------------------------------------\n"
SUMMARY=""
for label in $CELLS; do
  def=$(cell_def "$label"); [ -z "$def" ] && continue
  api="${def%%|*}"; rest="${def#*|}"; env_str="${rest%%|*}"; want="${rest#*|}"
  log="$OUT/$label.log"

  # --- positive control 1 (Linux only): is the driver present, and the RIGHT one?
  #
  # An EMPTY probe means one of two very different things: the driver is
  # absent, or there is no probe tool on this platform at all. Conflating them
  # skipped every cell on macOS and Windows, where vulkaninfo/glxinfo do not
  # exist. Only pre-flight when a probe is actually available; otherwise fall
  # through and let the post-run device banner decide.
  dev=""
  probe_ok=no
  case "$api" in
    vulkan) command -v vulkaninfo >/dev/null 2>&1 && probe_ok=yes ;;
    opengl) command -v glxinfo    >/dev/null 2>&1 && probe_ok=yes ;;
  esac
  if [ "$probe_ok" = yes ]; then
    dev=$(renderer_of "$api" "$env_str")
    if [ -z "$dev" ]; then
      printf "%-14s %-34s %s\n" "$label" "-" "SKIP (driver not present)"
      continue
    fi
    if ! echo "$dev" | grep -qiE "$want"; then
      printf "%-14s %-34s %s\n" "$label" "${dev:0:34}" "SKIP (wanted /$want/ — WRONG DEVICE)"
      continue
    fi
  fi

  # Banner probe: ctest only prints a test's stdout when it FAILS, so a clean
  # cell log contains no device banner at all. Run one test verbosely, purely
  # to capture which device this cell really got. Cheap, and it is the only
  # check that works on every platform -- the vulkaninfo/glxinfo pre-flight
  # above exists only on Linux.
  # shellcheck disable=SC2086
  env $env_str $( [ "$api" = vulkan ] && echo $VKVAL ) SCORE_TEST_API="$api" \
    ${TMO:+$TMO 600} ctest -R "$SCOPE" -E "$EXCL" -I 1,1 -V > "$log.banner" 2>&1

  if [ "$BATCH" = 1 ]; then
    # shellcheck disable=SC2086
    env $env_str $( [ "$api" = vulkan ] && echo $VKVAL ) SCORE_TEST_API="$api" \
      ${TMO:+$TMO $CELL_TIMEOUT} ctest -R "$SCOPE" -E "$EXCL" > "$log" 2>&1
    rc=$?
  else
    # ---- one test at a time: build it, run it, move on ---------------------
    : > "$log"; rc=0
    npass=0; nfail=0; nbuildfail=0; nskip=0
    tests=$(ctest -R "$SCOPE" -E "$EXCL" -N 2>/dev/null \
              | sed -n 's/^ *Test *#[0-9]*: *//p')
    for t in $tests; do
      free=$(free_gb)
      if [ -n "$free" ] && [ "$free" -lt "$MIN_FREE_GB" ]; then
        echo "!! STOPPING: only ${free} GB free on $BUILD (floor ${MIN_FREE_GB} GB)" >> "$log"
        echo "   built and ran $((npass+nfail)) of the cell's tests before stopping." >> "$log"
        rc=99; break
      fi
      tgt=$(target_for "$t")
      if [ -n "$tgt" ]; then
        if ! ${TMO:+$TMO 3600} ninja -C "$BUILD" -j"$BUILD_JOBS" "$tgt" >> "$log.build" 2>&1; then
          echo "BUILD-FAILED $t" >> "$log"; nbuildfail=$((nbuildfail+1)); continue
        fi
      fi
      # shellcheck disable=SC2086
      env $env_str $( [ "$api" = vulkan ] && echo $VKVAL ) SCORE_TEST_API="$api" \
        ${TMO:+$TMO $TEST_TIMEOUT} ctest -R "^$t\$" --output-on-failure >> "$log" 2>&1
      case $? in
        0) npass=$((npass+1)) ;;
        4) nskip=$((nskip+1)) ;;   # SKIP_RETURN_CODE
        *) nfail=$((nfail+1)) ;;
      esac

      # Delete this test's own artefacts before moving to the next one. Only
      # the executable and its object dir -- shared libs stay, since every
      # other test needs them.
      if [ "$PRUNE" = 1 ] && [ -n "$tgt" ]; then
        tdir=$(dirname "$tgt")
        rm -f  "$BUILD/$tgt"
        rm -rf "$BUILD/$tdir/CMakeFiles/$t.dir"
      fi
    done
    total=$((npass+nfail+nbuildfail))
    echo "PER-TEST SUMMARY: $npass passed, $nfail failed, $nbuildfail build-failed, $nskip skipped, of $total" >> "$log"
    echo "disk free after cell: $(free_gb) GB (prune=$PRUNE)" >> "$log"
  fi

  ran=$(grep -cE "\.\.\.\.* +(Passed|\*\*\*Failed|\*\*\*Exception|\*\*\*Skipped)" "$log" | tr -d " ")
  # Authoritative device check, every platform: what did score actually get?
  banner=$(device_from_log "$log.banner"); rhi_backend=$(backend_from_log "$log.banner")
  [ -z "$banner" ] && { banner=$(device_from_log "$log"); rhi_backend=$(backend_from_log "$log"); }
  if [ -n "$banner" ]; then dev="${banner#*device=\"}"; dev="${dev%\"}"; fi
  nullbe=""
  if [ -n "$rhi_backend" ] && echo "$rhi_backend" | grep -qi "null"; then
    nullbe=" !! QRhi::Null — VACUOUS PASS"
  fi
  # ctest prints "100% tests passed out of N" when nothing fails and
  # "N% tests passed, M tests failed out of N" otherwise. Accept both.
  line=$(grep -oE "[0-9]+% tests passed(, [0-9]+ tests failed)? out of [0-9]+" "$log" | tail -1)
  # Per-test mode writes its own roll-up; prefer it when present.
  pt=$(grep -m1 "^PER-TEST SUMMARY:" "$log" | sed 's/^PER-TEST SUMMARY: //')
  [ -n "$pt" ] && line="$pt"
  [ "$rc" = 99 ] && trunc_disk=" !! STOPPED — LOW DISK" || trunc_disk=""
  vuid=$(grep -oE "VUID-[A-Za-z0-9-]+" "$log" | sort -u | wc -l | tr -d " ")
  sync=$(grep -c "SYNC-HAZARD" "$log" | tr -d " ")
  asan=$(grep -c "ERROR: AddressSanitizer" "$log" | tr -d " ")
  ub=$(grep -c "runtime error:" "$log" | tr -d " ")
  # --- positive control 2: is the Vulkan validation layer actually LOADED?
  # Inferring this from the test log is unsound: a clean run legitimately
  # prints nothing at all. Ask the loader directly, under the cell's own env.
  lyr="n/a"
  if [ "$api" = vulkan ]; then
    # shellcheck disable=SC2086
    if env $env_str $VKVAL VK_LOADER_DEBUG=layer vulkaninfo --summary 2>&1 \
         | grep -qiE "Insert instance layer.*validation|VK_LAYER_KHRONOS_validation"; then
      lyr="layer:yes"
    else
      lyr="layer:NO(!)"
    fi
  fi
  # --- positive control 3: truncated run?
  trunc=""; [ "$rc" -eq 124 ] && trunc=" TIMED-OUT@${ran}/${NTESTS}"

  verdict="${line:-<no summary>}$trunc${trunc_disk:-}$nullbe"
  printf "%-14s %-34s %s\n" "$label" "${dev:0:34}" "$verdict"
  printf "%-14s   VUID=%-3s SYNC-HAZARD=%-4s ASan=%-3s UBSan=%-4s %s\n" "" "$vuid" "$sync" "$asan" "$ub" "$lyr"
  if [ "$vuid" -gt 0 ]; then
    grep -oE "VUID-[A-Za-z0-9-]+" "$log" | sort | uniq -c | sort -rn | head -5 | sed 's/^/                 /'
  fi
  sed -n '/The following tests FAILED/,$p' "$log" | grep -E "^\s+[0-9]+ - " | head -10 | sed 's/^/                 /'
  SUMMARY="$SUMMARY\n$label: $verdict [VUID=$vuid SYNC=$sync ASan=$asan UBSan=$ub $lyr]"
done

echo
echo "==============================================================="
echo " SUMMARY"
printf "%b\n" "$SUMMARY"
echo
echo " Reference baseline (gfx|threedim, recorded 2026-09): Vulkan validation"
echo " reports 0 VUID and 6 SYNC-HAZARD — the sync hazards are a known Qt"
echo " defect (ossia/sdk#35), NOT a score bug. Compare against that, not zero."
echo
echo " Under a Debug build the KHR_debug stream is flooded by one benign Qt"
echo " defect (glClear with no stencil buffer); count only messages naming a"
echo " failure."
echo " Full logs: $OUT"
echo "==============================================================="
