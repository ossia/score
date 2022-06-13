#!/bin/bash -eux

pacman -S --noconfirm \
   cmake ninja gcc llvm clang boost \
   icu \
   ffmpeg portaudio jack2 lv2 suil lilv sdl2 alsa-lib \
   avahi fftw bluez-libs \
   tar xz wget \
   brotli double-conversion zstd glib2 libb2 pcre2 \
   libxkbcommon libxkbcommon-x11 \
   vulkan-headers vulkan-icd-loader \
   mesa freetype2 harfbuzz fontconfig libglvnd \
   libdrm tslib udev zstd \
   xcb-proto xcb-util xcb-util-cursor xcb-util-image \
   xcb-util-keysyms xcb-util-renderutil xcb-util-wm \
   qt6-base qt6-shadertools qt6-tools qt6-5compat qt6-declarative qt6-scxml

source ci/common.deps.sh
