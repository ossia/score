#!/bin/bash -eux

cd /image

export CC=/usr/local/bin/gcc
export CXX=/usr/local/bin/g++
export CFLAGS="-O3"
export CXXFLAGS="-O3"
export NPROC=$(nproc)

yum -y install perl harfbuzz-devel which make perl-version libxcb libxcb-devel xcb-util xcb-util-devel fontconfig-devel libX11-devel libXrender-devel libXi-devel git dbus-devel glib2-devel mesa-libGL-devel

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.11.2
  git submodule update --init --recursive qtbase qtdeclarative qtquickcontrols2 qtserialport qtimageformats qtgraphicaleffects qtsvg qtwebsockets
)

cp /usr/local/bin/* /usr/bin/

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
                   -no-cups \
                   -no-iconv \
                   -no-tslib \
                   -no-icu \
                   -no-pch \
                   -ltcg \
                   -dbus-linked \
                   -no-system-proxies

  gmake -j$NPROC
  gmake install -j$NPROC
)

