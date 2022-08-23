#!/bin/bash -eux

apt-get update -qq
apt-get install -qq software-properties-common wget

wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /etc/apt/trusted.gpg.d/kitware.gpg >/dev/null
apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'

apt-get install -qq --force-yes \
     gcc-8 g++-8 \
     binutils clang-10 clang++-10 \
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
     libclang-10-dev

source ci/common.deps.sh

(
  cd 3rdparty/libossia/3rdparty/verdigris
  git remote -v
  PAGER=none git branch -a
  git status
  
  git fetch origin for_old_compilers
  git checkout FETCH_HEAD
)
