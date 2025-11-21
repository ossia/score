#!/bin/bash -eux

source ci/common.setup.sh

# See debian-builds.yml for packages
# (those are the specific compiler versions)
# Mainly this variable is neede because one needs to do
# apt install gcc-N g++-N to get g++-N,
# but only clang-M as clang++-M is not a package

export CLANG_VERSION=19

# libsdl2-dev libsdl2-2.0-0
$SUDO apt-get update -qq
$SUDO apt-get install -qq --force-yes \
     ${PACKAGES:=} \
     build-essential binutils cmake \
     libasound2-dev \
     ninja-build \
     clang-$CLANG_VERSION lld-$CLANG_VERSION libclang-$CLANG_VERSION-dev llvm-$CLANG_VERSION-dev \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libdrm-dev libgbm-dev \
     libdbus-1-dev \
     qt6-base-dev qt6-base-dev-tools qt6-base-private-dev \
     qt6-declarative-dev qt6-declarative-dev-tools qt6-declarative-private-dev \
     qt6-scxml-dev qt6-svg-dev qt6-connectivity-dev \
     qt6-websockets-dev \
     qt6-serialport-dev \
     qt6-shadertools-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libpipewire-0.3-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     libfmt-dev \
     libspdlog-dev \
     librubberband-dev \
     libsamplerate-dev \
     libsndfile-dev \
     libre2-dev \
     librapidfuzz-cpp-dev \
     libsnappy-dev \
     libboost-dev \
     libmagicenum-dev \
     libcoap3-dev \
     libhdf5-dev \
     libonnxruntime-dev \
     faust \
     libgps-dev \
     libsdl2-dev \
     glslang-dev glslang-tools \
     libzita-alsa-pcmi-dev \
     libvst3sdk-dev \
     puredata-dev \
     libpd-dev


source ci/common.deps.sh
