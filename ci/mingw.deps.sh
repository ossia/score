#!/bin/bash -eux

# Done in the GH action for caching:
# pacboy -S --needed --noconfirm \
#     cmake:p ninja:p toolchain:p \
#     qt6-base:p qt6-declarative:p qt6-websockets:p qt6-serialport:p \
#     qt6-shadertools:p qt6-5compat:p qt6-scxml:p qt6-tools:p \
#     boost:p portaudio:p fftw:p ffmpeg:p \
#     SDL2:p

# TODO:
# jack2:p   not available on clang?
# lv2 suil lilv

# source ci/common.deps.sh
