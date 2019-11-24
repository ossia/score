#!/bin/bash -eux

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi
if [[ "$CONF" == "tarball" ]];
then
  exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo chmod -R a+rwx /opt

    sudo apt-get update -qq
    sudo apt-get install wget software-properties-common

    wget -nv https://github.com/Kitware/CMake/releases/download/v3.16.0-rc4/cmake-3.16.0-rc4-Linux-x86_64.tar.gz -O cmake-linux.tgz &
    echo 'deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 15CF4D18AF4F7421

    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt-5.13.2-bionic

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes \
        g++-9 binutils libasound-dev ninja-build \
        gcovr lcov \
        qt513-meta-minimal qt513svg qt513quickcontrols2 qt513websockets qt513serialport \
        qt513base qt513declarative \
        libgl1-mesa-dev \
        libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
        portaudio19-dev clang-9 lld-9 \
        libbluetooth-dev \
        libsdl2-dev libsdl2-2.0-0 libglu1-mesa-dev libglu1-mesa \
        libgles2-mesa-dev \
        libavahi-compat-libdnssd-dev libsamplerate0-dev

    wait wget || true

    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake-latest
  ;;
  osx)
    set +e

    brew update
    brew install qt cmake portaudio ffmpeg ninja libsamplerate

#    SDK_ARCHIVE=homebrew-cache.txz
#    wget -nv https://github.com/OSSIA/score-sdk/releases/download/sdk12/$SDK_ARCHIVE -O $SDK_ARCHIVE
#    sudo mkdir -p /opt/score-sdk
#    sudo chmod -R a+rwx /opt/score-sdk
#    gtar xhaf $SDK_ARCHIVE --directory /opt/score-sdk

    set -e
  ;;
esac
