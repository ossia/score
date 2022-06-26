#!/bin/bash -eux

echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

apt-get update -qq
apt-get install -qq --force-yes \
    --allow-change-held-packages \
    --allow-downgrades \
    --allow-remove-essential \
    --allow-unauthenticated \
     binutils gcc-11 g++-11 \
     software-properties-common wget \
     libasound-dev \
     ninja-build cmake \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qtbase5-dev qtdeclarative5-dev libqt5serialport5-dev libqt5websockets5-dev libqt5opengl5-dev \
     qtbase5-private-dev qtdeclarative5-private-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
     file

source ci/common.deps.sh
