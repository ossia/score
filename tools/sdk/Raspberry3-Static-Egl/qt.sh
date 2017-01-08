#!/bin/bash -eux

export NPROC=$(nproc)
export PATH=/opt/gcc-6/bin:$PATH
export CC=/opt/gcc-6/bin/gcc
export CXX=/opt/gcc-6/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-6/lib
export PKG_CONFIG_LIBDIR=/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/lib/pkgconfig:/usr/share/pkgconfig

apt-get install gstreamer1.0-omx

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.8
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
                   -opengl es2 \
                   -device linux-rpi3-g++ \
                   -prefix /opt/qt5-static \
                   -device-option CROSS_COMPILE=/opt/gcc-6/bin/ \
                   -sysroot / \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit -skip qtwebengine

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
                   -opengl es2 \
                   -device linux-rpi3-g++ \
                   -prefix /opt/qt5-dynamic \
                   -device-option CROSS_COMPILE=/opt/gcc-6/bin/ \
                   -sysroot / \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit -skip qtwebengine

  make -j$NPROC
  make install -j$NPROC
)

rm -rf qt5-build-dynamic
rm -rf qt5
