#!/bin/bash -eux

export NPROC=$(nproc)
export PATH=/opt/gcc-7/bin:$PATH
export CC=/opt/gcc-7/bin/gcc
export CXX=/opt/gcc-7/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-7/lib64

apt-get -y --force-yes install libsdl2-dev libgstreamer1.0-dev libgstreamer-.*1.0-dev libssl-dev libglib2.0-dev libicu-dev libfontconfig-dev libxcb.*dev  libpng-dev libjpeg-dev zlib1g-dev libpulse-dev "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev flex bison gperf libicu-dev libxslt-dev ruby perl python  libasound2-dev libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev libfontconfig1-dev libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev libdbus-1-dev openssl libdbus-1-dev libgstreamer1.0-dev libopenal-dev libbluetooth-dev libsdl2-dev libts-dev libmtdev-dev libfontconfig1-dev libsctp-dev libmysqlclient-dev libgstreamer-plugins-base1.0-dev libssl*-dev

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.9.1
  git submodule update --init --recursive
)


mkdir qt5-build-static
(
  cd qt5-build-static
  ../qt5/configure -release \
                   -opensource \
                   -confirm-license \
                   -nomake examples \
                   -nomake tests \
                   -static \
                   -no-compile-examples \
                   -ltcg \
                   -strip -skip qtwebkit -skip qtscript \
                   -prefix /opt/qt5-static 

  make -j$NPROC
  make install -j$NPROC
)
rm -rf qt5-build-static

mkdir qt5-build-dynamic
(
  cd qt5-build-dynamic
  ../qt5/configure -release \
                   -opensource \
                   -confirm-license \
                   -nomake examples \
                   -nomake tests \
                   -no-compile-examples \
                   -ltcg \
                   -strip -skip qtwebkit -skip qtscript \
                   -prefix /opt/qt5-dynamic 

  make -j$NPROC
  make install -j$NPROC
)

rm -rf qt5-build-dynamic
rm -rf qt5
