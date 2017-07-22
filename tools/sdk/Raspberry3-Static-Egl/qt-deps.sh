#!/bin/bash -eux

export NPROC=$(nproc)
export PATH=/opt/gcc-7/bin:$PATH
export CC=/opt/gcc-7/bin/gcc
export CXX=/opt/gcc-7/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-7/lib
export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig

apt-get -y --force-yes install gstreamer1.0-omx

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout v5.9.1
  git submodule update --init --recursive
)
