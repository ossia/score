#!/usr/bin/env bash
# CSF / ISF shader render-regression: load every shader tester into its process
# and render it on BOTH GL backends (llvmpipe and the machine's GPU via X11),
# recording whether it compiles and renders without crashing.
#
#   tests/integration/csf-sweep.sh [corpus-dir]     (run from the build root)
#
# Default corpus: the csf-testers/ folder of the csf-examples package. Pass a
# directory to sweep any set of .fs/.frag/.glsl/.cs/.comp/.csf shaders.
#
# Per file, ObjectGallery --shader is invoked once per backend, isolated in its
# own process so a shader compile-failure or render crash is attributed to that
# file/backend. Exit 0 = ok, 3 = no texture output (skipped), 5 = unsupported,
# anything else / signal / timeout = failure.
set -u
GALLERY="tests/integration/ObjectGallery"
CORPUS="${1:-$HOME/Documents/ossia/score/packages/csf-examples/csf-testers}"
DISP="${DISPLAY:-:0}"
SECS=1

run() { # file backend_env...
  local file="$1"; shift
  env DISPLAY="$DISP" SCORE_AUDIO_BACKEND=dummy "$@" \
    timeout 40 "$GALLERY" --shader "$file" --seconds "$SECS" >/dev/null 2>&1
}

mapfile -t FILES < <(find "$CORPUS" -type f \
  \( -name '*.fs' -o -name '*.frag' -o -name '*.glsl' \
     -o -name '*.cs' -o -name '*.comp' -o -name '*.csf' \) | sort)

echo "Rendering ${#FILES[@]} shaders from $CORPUS on llvmpipe + NVIDIA(X11)..."
# Two separate passes: mixing software (llvmpipe) and the NVIDIA driver on the
# same X server back-to-back leaves GL state that breaks the next context, so
# run every file on one backend, then every file on the other.
declare -A SW HW
for f in "${FILES[@]}"; do run "$f" LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe; SW["$f"]=$?; done
for f in "${FILES[@]}"; do run "$f"; HW["$f"]=$?; done

fail_sw=0; fail_hw=0; ok=0; skip=0
for f in "${FILES[@]}"; do
  sw=${SW["$f"]}; hw=${HW["$f"]}
  s=OK; [ "$sw" = 3 ] && s=skip; [ "$sw" = 5 ] && s=unsup
  [ "$sw" != 0 ] && [ "$sw" != 3 ] && [ "$sw" != 5 ] && { s="FAIL($sw)"; fail_sw=$((fail_sw+1)); }
  h=OK; [ "$hw" = 3 ] && h=skip; [ "$hw" = 5 ] && h=unsup
  [ "$hw" != 0 ] && [ "$hw" != 3 ] && [ "$hw" != 5 ] && { h="FAIL($hw)"; fail_hw=$((fail_hw+1)); }
  [ "$sw" = 0 ] && [ "$hw" = 0 ] && ok=$((ok+1))
  { [ "$sw" = 3 ] || [ "$hw" = 3 ]; } && skip=$((skip+1))
  [ "$s" = OK ] && [ "$h" = OK ] || printf "%-46s llvmpipe=%-9s nvidia=%-9s\n" "$(basename "$f")" "$s" "$h"
done
echo "---"
echo "rendered-ok(both)=$ok  skipped=$skip  failures: llvmpipe=$fail_sw nvidia=$fail_hw  (of ${#FILES[@]})"
[ "$fail_sw" = 0 ] && [ "$fail_hw" = 0 ]
