#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

if [[ -z "$PKGS" ]]; then
  echo "PKGS must be defined (for instance PKGS=clang-21, PKGS=gcc...)"
  exit 1
fi

# For newer CMake
$SUDO apt install -y ca-certificates gpg wget lsb-release software-properties-common
$SUDO apt update -qq
$SUDO apt install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     $PKGS \
     build-essential \
     binutils \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libdbus-1-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev qt6-svg-dev qt6-connectivity-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libclang-dev llvm-dev \
     libvulkan-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev


source ci/common.deps.sh LINUX
