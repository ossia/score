#!/bin/bash -eux

sudo chmod -R a+rwx /opt

sudo apt-get update -qq
sudo apt-get install wget software-properties-common

wget -nv https://github.com/jcelerier/cninja/releases/download/v3.7.4/cninja-v3.7.4-Linux.tar.gz -O cninja.tgz &
echo 'deb http://apt.llvm.org/focal/ llvm-toolchain-focal-10 main' | sudo tee /etc/apt/sources.list.d/llvm.list
sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 15CF4D18AF4F7421

sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
sudo add-apt-repository --yes ppa:beineri/opt-qt-5.15.0-focal

sudo apt-get update -qq
sudo apt-get install -qq --force-yes \
    g++-10 binutils libasound-dev ninja-build cmake \
    gcovr lcov \
    qt515base qt515declarative qt515svg qt515quickcontrols2 qt515websockets qt515serialport \
    libgl1-mesa-dev \
    libavcodec-dev libavdevice-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev \
    portaudio19-dev \
    clang-10 lld-10 libc++-10-dev libc++abi-10-dev \
    libbluetooth-dev \
    libsdl2-dev libsdl2-2.0-0 libglu1-mesa-dev libglu1-mesa \
    libgles2-mesa-dev \
    libavahi-compat-libdnssd-dev libsamplerate0-dev \
    libclang-10-dev

wait wget || true

tar xaf cninja.tgz
sudo cp -rf cninja /usr/bin/
