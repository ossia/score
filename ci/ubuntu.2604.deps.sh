#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

# PKGS holds the compiler and the LLVM / libclang development packages the JIT
# plug-in builds against, e.g. "libclang-dev llvm-dev" for the distribution's.
if [[ -z "$PKGS" ]]; then
  echo "PKGS must be defined (for instance PKGS=\"clang-22 libclang-22-dev llvm-22-dev\")"
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
     libx11-dev libxext-dev libxcomposite-dev libxrandr-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev qt6-svg-dev qt6-connectivity-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev libsdl3-dev libsdl3-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libvulkan-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file \
     dpkg-dev


source ci/common.deps.sh LINUX
