#!/bin/bash -eux

source ci/common.setup.sh

$SUDO echo 'deb http://deb.debian.org/debian buster-backports main contrib non-free' >> /etc/apt/sources.list

$SUDO apt-get update -qq
$SUDO apt-get -t buster-backports install -qq --force-yes cmake
$SUDO apt-get install -qq --force-yes \
     binutils gcc g++ clang \
     libasound-dev \
     ninja-build \
     libfftw3-dev \
     libsuil-dev liblilv-dev lv2-dev \
     qtbase5-dev qtdeclarative5-dev libqt5serialport5-dev libqt5websockets5-dev libqt5opengl5-dev \
     qtbase5-private-dev qtdeclarative5-private-dev \
     libbluetooth-dev libsdl2-dev libsdl2-2.0-0 \
     libglu1-mesa-dev libglu1-mesa libgles2-mesa-dev \
     libavahi-compat-libdnssd-dev libsamplerate0-dev \
     portaudio19-dev \
     libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev

source ci/common.deps.sh

(
  cd 3rdparty/libossia/3rdparty/verdigris
  git remote -v
  PAGER=none git branch -a
  git status
  
  git fetch origin for_old_compilers
  git checkout FETCH_HEAD
)
