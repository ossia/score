#!/bin/bash -eux

sudo pacman -S --needed --noconfirm \
    mingw-w64-qt5-base mingw-w64-qt5-declarative mingw-w64-qt5-serialport mingw-w64-qt5-websockets \
    mingw-w64-portaudio \
    mingw-w64-fftw \
    mingw-w64-ffmpeg \
    ninja \
    mingw-w64-gcc

# sudo pacman -S --needed --noconfirm \
#     mingw-w64-x86_64-qt5 \
#     mingw-w64-x86_64-portaudio \
#     mingw-w64-x86_64-fftw \
#     mingw-w64-x86_64-ffmpeg \
#     mingw-w64-x86_64-cmake \
#     mingw-w64-x86_64-ninja \
#     mingw-w64-x86_64-gcc