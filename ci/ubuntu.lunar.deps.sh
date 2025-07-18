#!/bin/bash -eux

source ci/common.setup.sh

echo 'debconf debconf/frontend select Noninteractive' | $SUDO debconf-set-selections

wget -nv https://github.com/xpack-dev-tools/gcc-xpack/releases/download/v14.2.0-2/xpack-gcc-14.2.0-2-linux-x64.tar.gz
tar xaf xpack-gcc-14.2.0-2-linux-x64.tar.gz
rm xpack-gcc-14.2.0-2-linux-x64.tar.gz
$SUDO mv xpack-gcc-14.2.0-2 /opt/gcc-14

$SUDO apt-get update -qq
$SUDO apt-get install -y \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc g++ clang lld \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libdbus-1-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev \
     libqt6opengl6-dev \
     libqt6websockets6-dev \
     libqt6serialport6-dev \
     libqt6shadertools6-dev \
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

source ci/common.deps.sh
