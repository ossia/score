#!/bin/bash -eux

source ci/common.setup.sh

echo 'deb http://deb.debian.org/debian bullseye-backports main' | $SUDO tee -a /etc/apt/sources.list
# libsdl2-dev libsdl2-2.0-0
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes \
     binutils gcc g++ clang cmake \
     libasound-dev \
     ninja-build \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libclang-dev llvm-dev \
     libdrm-dev libgbm-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     libqt6core5compat6-dev \
     libqt6opengl6-dev \
     libqt6websockets6-dev \
     libqt6serialport6-dev \
     libqt6shadertools6-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

source ci/common.deps.sh
