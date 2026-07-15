#!/usr/bin/env bash
# Render-regression sweep: render each object on BOTH GL backends and record
# whether it renders without crashing. Run from the build root.
#
#   tests/integration/render-sweep.sh [name-filter]
#
# For each object (optionally filtered), invokes ObjectGallery --render once per
# backend, isolated in its own process so a render-path crash is attributed to
# that object/backend. Exit codes: 0 = rendered ok, 3 = no texture output
# (skipped), anything else / signal / timeout = render failure.
set -u
GALLERY="tests/integration/ObjectGallery"
FILTER="${1:-}"
SECS=1
DISP="${DISPLAY:-:0}"

# GL backends: software (llvmpipe) and the machine's real GPU via X11.
run_one() { # name backend_env...
  local name="$1"; shift
  local out
  out=$(DISPLAY="$DISP" SCORE_AUDIO_BACKEND=dummy "$@" \
    timeout 40 "$GALLERY" --render "$name" --seconds "$SECS" 2>/dev/null)
  local rc=$?
  printf '%s\n' "$out" | grep '^RENDER'
  return $rc
}

mapfile -t NAMES < <(DISPLAY="$DISP" "$GALLERY" --list "$FILTER" 2>/dev/null \
                       | sed -n 's/  *[0-9a-f-]\{36\}$//p' | sed 's/ *$//' | sort -u)
echo "Sweeping ${#NAMES[@]} objects on llvmpipe + $( [ -n "${NV:-}" ] && echo hw ) NVIDIA(X11)..."
crash_sw=0; crash_hw=0; ok=0; skip=0
for name in "${NAMES[@]}"; do
  [ -z "$name" ] && continue
  run_one "$name" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe >/dev/null; sw=$?
  run_one "$name" >/dev/null; hw=$?
  tag_sw=OK; [ "$sw" = 3 ] && tag_sw=skip; [ "$sw" != 0 ] && [ "$sw" != 3 ] && { tag_sw="CRASH($sw)"; crash_sw=$((crash_sw+1)); }
  tag_hw=OK; [ "$hw" = 3 ] && tag_hw=skip; [ "$hw" != 0 ] && [ "$hw" != 3 ] && { tag_hw="CRASH($hw)"; crash_hw=$((crash_hw+1)); }
  [ "$sw" = 0 ] && [ "$hw" = 0 ] && ok=$((ok+1))
  { [ "$sw" = 3 ] || [ "$hw" = 3 ]; } && skip=$((skip+1))
  printf "%-42s llvmpipe=%-10s nvidia=%-10s\n" "$name" "$tag_sw" "$tag_hw"
done
echo "---"
echo "ok=$ok skip=$skip  crashes: llvmpipe=$crash_sw nvidia=$crash_hw"
[ "$crash_sw" = 0 ] && [ "$crash_hw" = 0 ]
