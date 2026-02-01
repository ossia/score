#!/bin/bash -eux

source ci/common.setup.sh

export CLANG_VERSION=19
echo "deb http://deb.debian.org/debian bookworm-backports main" | $SUDO tee -a /etc/apt/sources.list

$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes wget lsb-release software-properties-common gnupg
wget -nv https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
$SUDO ./llvm.sh $CLANG_VERSION

wget -nv https://github.com/xpack-dev-tools/gcc-xpack/releases/download/v14.2.0-2/xpack-gcc-14.2.0-2-linux-x64.tar.gz
tar xaf xpack-gcc-14.2.0-2-linux-x64.tar.gz
rm xpack-gcc-14.2.0-2-linux-x64.tar.gz
$SUDO mv xpack-gcc-14.2.0-2 /opt/gcc-14

# libsdl2-dev libsdl2-2.0-0
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes \
     binutils gcc g++ cmake/bookworm-backports \
     libasound-dev \
     ninja-build \
     libclang-$CLANG_VERSION-dev llvm-$CLANG_VERSION-dev \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libdbus-1-dev \
     libdrm-dev libgbm-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev qt6-svg-dev qt6-connectivity-dev \
     libqt6opengl6-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

source ci/common.deps.sh LINUX
