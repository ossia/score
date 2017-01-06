#!/bin/bash -eux

cd /image
rm -rf gcc-build-2

export PATH=/opt/gcc-6/bin:$PATH
NPROC=$(nproc)

rsync -a /opt/vc/ /usr/

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.8
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
                   -pkg-config \
                   -dbus-linked \
                   -opengl es2 \
                   -device linux-rpi3-g++ \
                   -prefix /opt/gcc-6 \
                   -device-option CROSS_COMPILE=/usr/bin/ \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit

  make -j$NPROC
  make install -j$NPROC
)
rm -rf qt5-build
