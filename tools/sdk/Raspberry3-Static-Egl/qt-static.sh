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
                   -strip \
                   -opengl es2 \
                   -device linux-rasp-pi3-vc4-g++ \
                   -prefix /opt/qt5-static \
                   -device-option CROSS_COMPILE=/opt/gcc-7/bin/ \
                   -sysroot / \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit

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
                   -strip \
                   -opengl es2 \
                   -device linux-rpi3-g++ \
                   -prefix /opt/qt5-dynamic \
                   -device-option CROSS_COMPILE=/opt/gcc-7/bin/ \
                   -sysroot / \
                   -skip qtwayland -skip webkit -skip wayland -skip qtscript -skip qtwebkit

  make -j$NPROC
  make install -j$NPROC
)

rm -rf qt5-build-dynamic
rm -rf qt5
