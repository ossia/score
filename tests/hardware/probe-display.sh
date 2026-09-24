#!/bin/sh
# Probe: is a real display session available? The score->score PipeWire cells
# render through the actual GPU; software GL (llvmpipe offscreen) is too slow
# and lossy to ever pass the PSNR gate, so those cells are display-gated.
# Exit 0 = present, non-zero = absent (wrapper turns that into ctest SKIP 77).
[ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ] || exit 1

# A display is not a GPU: Xvfb's GLX is llvmpipe. Reject software renderers
# when glxinfo can tell; otherwise the display check above stands.
if [ -n "${DISPLAY:-}" ] && command -v glxinfo >/dev/null 2>&1; then
  if renderer=$(glxinfo -B 2>/dev/null | grep -i 'OpenGL renderer string'); then
    case "$renderer" in
      *[Ll][Ll][Vv][Mm][Pp][Ii][Pp][Ee]* | *[Ss][Oo][Ff][Tt][Pp][Ii][Pp][Ee]* | *[Ss][Ww][Rr][Aa][Ss][Tt]*)
        exit 1 ;;
    esac
  fi
fi

exit 0
