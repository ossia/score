#!/bin/bash -eux

# libsdl2-dev libsdl2-2.0-0 
apt-get update -qq
apt-get install -qq --force-yes \
     binutils gcc g++ clang cmake \
     libasound-dev \
     ninja-build \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     libclang-dev llvm-dev \
     libdrm-dev libgbm-dev \
     qtbase5-dev qtdeclarative5-dev libqt5serialport5-dev libqt5websockets5-dev libqt5opengl5-dev \
     qtbase5-private-dev qtdeclarative5-private-dev \
     libbluetooth-dev \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

