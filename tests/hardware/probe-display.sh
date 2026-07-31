#!/bin/sh
# Probe: is a real display session available? The score->score PipeWire cells
# render through the actual GPU; software GL (llvmpipe offscreen) is too slow
# and lossy to ever pass the PSNR gate, so those cells are display-gated.
[ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]
