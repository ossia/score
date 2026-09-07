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

if [ -z "${DISPLAY:-}" ]; then export DISPLAY=:0; fi
if [ -z "${XAUTHORITY:-}" ]; then
  for x in "$HOME/.Xauthority" /run/user/$(id -u)/lyxauth /run/user/$(id -u)/gdm/Xauthority; do
    [ -f "$x" ] && { export XAUTHORITY="$x"; break; }
  done
fi
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

# Vulkan goggles: core validation + synchronization + best practices.
VKVAL='VK_LOADER_LAYERS_ENABLE=*validation* VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT'

# ---- cell definitions -------------------------------------------------------
# label | SCORE_TEST_API | env | expected renderer regex
cell_def() {
  case "$1" in
    vk-nvidia)   echo "vulkan|VK_LOADER_DRIVERS_SELECT=nvidia*|NVIDIA|GeForce|Quadro|RTX" ;;
    vk-intel)    echo "vulkan|VK_LOADER_DRIVERS_SELECT=intel*|Intel|ARL|Arc|Iris" ;;
    vk-lavapipe) echo "vulkan|VK_LOADER_DRIVERS_SELECT=lvp*|llvmpipe|lavapipe" ;;
    vk-amd)      echo "vulkan|VK_LOADER_DRIVERS_SELECT=radeon*|AMD|RADV" ;;
    gl-nvidia)   echo "opengl|__NV_PRIME_RENDER_OFFLOAD=1 __GLX_VENDOR_LIBRARY_NAME=nvidia|NVIDIA|GeForce|Quadro|RTX" ;;
    gl-intel)    echo "opengl|__GLX_VENDOR_LIBRARY_NAME=mesa MESA_LOADER_DRIVER_OVERRIDE=iris|Intel|ARL|Arc|Iris" ;;
    gl-llvmpipe) echo "opengl|__GLX_VENDOR_LIBRARY_NAME=mesa LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe|llvmpipe" ;;
    *) echo "" ;;
  esac
}
ALL_CELLS="vk-nvidia vk-intel vk-lavapipe vk-amd gl-nvidia gl-intel gl-llvmpipe"
CELLS="${CELLS:-$ALL_CELLS}"

mkdir -p "$OUT"
cd "$BUILD" || { echo "!! BUILD=$BUILD is not a directory"; exit 1; }
[ -f CTestTestfile.cmake ] || { echo "!! $BUILD is not a CMake build dir"; exit 1; }

NTESTS=$(ctest -R "$SCOPE" -E "$EXCL" -N 2>/dev/null | sed -n 's/^Total Tests: //p')
[ -z "$NTESTS" ] && NTESTS=0
if [ "$NTESTS" -eq 0 ]; then echo "!! no tests match scope '$SCOPE'"; exit 1; fi

# Budget from a measured probe unless told otherwise: run one cheap test and
# scale, floored at 30 s/test so a fast probe cannot under-budget a slow suite.
if [ -z "$SECONDS_PER_TEST" ]; then
  echo "-- probing per-test cost..."
  t0=$SECONDS
  ctest -R "$SCOPE" -E "$EXCL" -N >/dev/null 2>&1
  ctest -R "$SCOPE" -E "$EXCL" --stop-on-failure -I 1,1 >/dev/null 2>&1
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

renderer_of() { # $1 = api, $2 = env
  if [ "$1" = "vulkan" ]; then
    env $2 vulkaninfo --summary 2>/dev/null | grep -m1 "deviceName" | sed 's/.*= //'
  else
    env $2 glxinfo -B 2>/dev/null | grep -m1 "OpenGL renderer" | sed 's/.*: //'
  fi
}

printf "\n%-14s %-34s %s\n" "CELL" "DEVICE (positive control)" "VERDICT"
printf -- "---------------------------------------------------------------------------\n"
SUMMARY=""
for label in $CELLS; do
  def=$(cell_def "$label"); [ -z "$def" ] && continue
  api="${def%%|*}"; rest="${def#*|}"; env_str="${rest%%|*}"; want="${rest#*|}"
  log="$OUT/$label.log"

  # --- positive control 1: is the driver even present, and is it the RIGHT one?
  dev=$(renderer_of "$api" "$env_str")
  if [ -z "$dev" ]; then
    printf "%-14s %-34s %s\n" "$label" "-" "SKIP (driver not present)"
    continue
  fi
  if ! echo "$dev" | grep -qiE "$want"; then
    printf "%-14s %-34s %s\n" "$label" "${dev:0:34}" "SKIP (wanted /$want/ — WRONG DEVICE)"
    continue
  fi

  # shellcheck disable=SC2086
  env $env_str $( [ "$api" = vulkan ] && echo $VKVAL ) SCORE_TEST_API="$api" \
    timeout "$CELL_TIMEOUT" ctest -R "$SCOPE" -E "$EXCL" > "$log" 2>&1
  rc=$?

  ran=$(grep -cE "\.\.\.\.* +(Passed|\*\*\*Failed|\*\*\*Exception|\*\*\*Skipped)" "$log")
  # ctest prints "100% tests passed out of N" when nothing fails and
  # "N% tests passed, M tests failed out of N" otherwise. Accept both.
  line=$(grep -oE "[0-9]+% tests passed(, [0-9]+ tests failed)? out of [0-9]+" "$log" | tail -1)
  vuid=$(grep -oE "VUID-[A-Za-z0-9-]+" "$log" | sort -u | wc -l)
  sync=$(grep -c "SYNC-HAZARD" "$log")
  asan=$(grep -c "ERROR: AddressSanitizer" "$log")
  ub=$(grep -c "runtime error:" "$log")
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

  verdict="${line:-<no summary>}$trunc"
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
