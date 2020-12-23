#!/bin/bash -eux

pacman -S --noconfirm \
   cmake ninja gcc llvm clang boost \
   qt5-base qt5-imageformats qt5-websockets qt5-serialport qt5-declarative qt5-tools icu \
   ffmpeg portaudio jack2 lv2 suil lilv sdl2 alsa-lib \
   avahi fftw bluez-libs

