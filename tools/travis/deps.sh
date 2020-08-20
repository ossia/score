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

    wget -nv https://github.com/jcelerier/cninja/releases/download/v3.5.0/cninja-3.5.0-Linux.tar.gz -O cninja-linux.tgz &
    wget -nv https://github.com/Kitware/CMake/releases/download/v3.18.1/cmake-3.18.1-Linux-x86_64.tar.gz -O cmake-linux.tgz &
    echo 'deb http://apt.llvm.org/focal/ llvm-toolchain-focal-10 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 15CF4D18AF4F7421

    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt-5.15.0-focal

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes \
        g++-10 binutils libasound-dev ninja-build \
        gcovr lcov \
        qt515base qt515declarative qt515svg qt515quickcontrols2 qt515websockets qt515serialport \
        libgl1-mesa-dev \
        libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
        portaudio19-dev clang-10 lld-10 \
        libbluetooth-dev \
        libsdl2-dev libsdl2-2.0-0 libglu1-mesa-dev libglu1-mesa \
        libgles2-mesa-dev \
        libavahi-compat-libdnssd-dev libsamplerate0-dev

    wait wget || true

    tar xaf cninja-linux.tgz
    cp -rf cninja /usr/bin/
    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake-latest
  ;;
  osx)
    set +e

    brew update
    brew remove qt
    brew upgrade 
    brew install gnu-tar wget
##     brew install qt cmake portaudio ffmpeg ninja libsamplerate
    SDK_ARCHIVE=score-sdk-mac.tar.gz
    wget -nv https://github.com/ossia/score-sdk/releases/download/sdk15/$SDK_ARCHIVE -O $SDK_ARCHIVE
    sudo mkdir -p /opt/score-sdk-osx
    sudo chmod -R a+rwx /opt/score-sdk-osx
    gtar xhaf $SDK_ARCHIVE --directory /opt/score-sdk-osx
    sudo rm -rf /Library/Developer/CommandLineTools
    sudo rm -rf /usr/local/include/c++
    set -e
  ;;
esac
