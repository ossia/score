#!/bin/bash -eux

# Done in the GH action for caching:
# pacboy -S --needed --noconfirm \
#     cmake:p ninja:p toolchain:p cppwinrt:p qt6-base:p qt6-declarative:p qt6-websockets:p qt6-serialport:p qt6-shadertools:p qt6-scxml:p qt6-tools:p qt6-svg:p qt6-connectivity:p boost:p portaudio:p fftw:p ffmpeg:p SDL2:p

# TODO:
# jack2:p   not available on clang?
# lv2 suil lilv


# if c++ --version | grep clang; then
  source ci/common.deps.sh WIN32
#fi

# The MSYS2 build disables FetchContent downloads; stage the pinned source here.
essentia_archive="$(mktemp)"
trap 'rm -f -- "$essentia_archive"' EXIT
curl -fsSL https://codeload.github.com/MTG/essentia/tar.gz/23ae7baddecfc3227ba23ad69fa11f73ac5293a4 -o "$essentia_archive"
printf '%s  %s\n' 41d8ce1308b3471059c67f9e6c857a97e766dfb0b0b741cfedea6db624738c32 "$essentia_archive" | sha256sum -c
mkdir -p build/essentia-source
tar -xf "$essentia_archive" --strip-components=1 -C build/essentia-source
