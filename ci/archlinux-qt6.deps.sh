#!/bin/bash -eux

pacman -S --noconfirm \
   cmake ninja gcc llvm clang boost \
   icu \
   ffmpeg portaudio jack2 lv2 suil lilv sdl2 alsa-lib \
   avahi fftw bluez-libs \
   tar xz wget \
   libxkbcommon libxkbcommon-x11 \
   vulkan-headers vulkan-icd-loader 

wget https://github.com/ossia/sdk/releases/download/sdk20/qt6-debug.tar.xz
tar xaf qt6-debug.tar.xz
ls
mkdir -p /opt/
mv qt6 /opt/

