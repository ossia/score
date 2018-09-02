#!/bin/bash -eux

cd /image

export PATH=/usr/local/bin:$PATH
NPROC=$(nproc)

apt-get -y install "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev flex bison gperf libicu-dev libxslt-dev ruby perl python  libasound2-dev libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev libfontconfig1-dev libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev libdbus-1-dev libssl-dev openssl

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.11.2
  perl init-repository --module-subset=qtbase,qtimageformats,qtsvg,qtwebsockets,qttranslations,qtrepotools,qtdeclarative,qttools,qtdoc
)

export CC=/usr/local/bin/gcc
export CXX=/usr/local/bin/g++
export CFLAGS="-O3 -march=armv7-a -g0"
export CXXFLAGS="-O3 -march=armv7-a -g0"
export LD_LIBRARY_PATH=/usr/local/lib

mkdir qt5-build
(
  cd qt5-build
  ../qt5/configure -release \
                   -opensource \
                   -confirm-license \
                   -nomake examples \
                   -nomake tests \
                   -no-compile-examples \
                   -no-qml-debug \
                   -qt-zlib \
                   -no-mtdev \
                   -no-journald \
                   -no-syslog \
                   -no-gif \
                   -qt-libpng \
                   -qt-libjpeg \
                   -qt-zlib \
                   -qt-freetype \
                   -qt-harfbuzz \
                   -openssl \
                   -qt-pcre \
                   -qt-xcb \
                   -qt-xkbcommon-x11 \
                   -no-xinput2 \
                   -glib \
                   -no-pulseaudio \
                   -no-alsa \
                   -no-cups \
                   -no-iconv \
                   -no-tslib \
                   -no-icu \
                   -no-pch \
                   -ltcg \
                   -dbus-linked \
                   -no-gstreamer \
                   -no-system-proxies 

  make -j$NPROC
  make install -j$NPROC
)
