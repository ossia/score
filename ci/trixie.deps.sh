#!/bin/bash -eux

source ci/common.setup.sh

# See debian-builds.yml for packages
# (those are the specific compiler versions)
# Mainly this variable is neede because one needs to do
# apt install gcc-N g++-N to get g++-N,
# but only clang-M as clang++-M is not a package

# libsdl2-dev libsdl2-2.0-0
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes \
     ${PACKAGES:=} \
     build-essential binutils clang cmake \
     libasound2-dev \
     ninja-build \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libclang-dev llvm-dev \
     libdrm-dev libgbm-dev \
     libdbus-1-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     qt6-svg-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

source ci/common.deps.sh
