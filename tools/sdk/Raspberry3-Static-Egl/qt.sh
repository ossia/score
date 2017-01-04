#!/bin/bash -eux

cd /image

export PATH=/opt/gcc-6/bin:$PATH
NPROC=$(nproc)

apt-get -y install "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev flex bison gperf libicu-dev libxslt-dev ruby perl python  libasound2-dev libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev libfontconfig1-dev libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev libdbus-1-dev libssl-dev openssl libraspberrypi0 libraspberrypi-dev libraspberrypi-doc libraspberrypi-bin

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.7.1
  git submodule update --init --recursive
)

export CC=/opt/gcc-6/bin/gcc
export CXX=/opt/gcc-6/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-6/lib

mkdir qt5-build
(
  cd qt5-build
  ../qt5/configure -release \
                   -opensource \
                   -confirm-license \
                   -nomake examples \
                   -nomake tests \
                   -static \
                   -no-compile-examples \
                   -ltcg \
                   -dbus-linked \
                   -opengl es2 \
                   -device linux-rpi3-g++ \
                   -prefix /opt/gcc-6 \
                   -device-option CROSS_COMPILE=/usr/bin/ \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit 

  make -j$NPROC
  make install -j$NPROC
)
