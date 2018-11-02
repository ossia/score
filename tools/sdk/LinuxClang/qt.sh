#!/bin/bash -eux

cd /image

export CC=/usr/local/bin/clang
export CXX=/usr/local/bin/clang++
export CFLAGS="-O3 -stdlib=libc++"
export CXXFLAGS="-O3 -stdlib=libc++"
export NPROC=$(nproc)

git clone https://code.qt.io/qt/qt5.git

(
  cd qt5
  git checkout 5.12
  git submodule update --init --recursive qtbase qtdeclarative qtquickcontrols2 qtserialport qtimageformats qtgraphicaleffects qtsvg qtwebsockets
  
  sed -i 's/fuse-ld=gold/fuse-ld=lld/g' \
    qtbase/mkspecs/common/gcc-base-unix.conf \
    qtbase/mkspecs/features/qt_configure.prf \
    qtbase/configure.json     
)

# cp /usr/local/bin/* /usr/bin/
export LD_LIBRARY_PATH=/usr/local/lib

mkdir qt5-build-dynamic
(
  cd qt5-build-dynamic
  ../qt5/configure -release \
                   -c++std c++1z \
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
                   -qt-pcre \
                   -qt-xcb \
                   -qt-xkbcommon-x11 \
                   -glib \
                   -no-cups \
                   -no-iconv \
                   -no-tslib \
                   -no-icu \
                   -no-pch \
                   -ltcg \
                   -openssl-linked \
                   -dbus-linked \
                   -no-system-proxies \
                   -platform linux-clang-libc++ \
                   -prefix /opt/qt5-dynamic

  gmake -j$NPROC
  gmake install -j$NPROC
  rm -rf qt5-build-dynamic
)


mkdir qt5-build-static
(
  cd qt5-build-static
  ../qt5/configure -release \
                   -c++std c++1z \
                   -static \
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
                   -qt-pcre \
                   -qt-xcb \
                   -qt-xkbcommon-x11 \
                   -glib \
                   -no-cups \
                   -no-iconv \
                   -no-tslib \
                   -no-icu \
                   -no-pch \
                   -ltcg \
                   -openssl-linked \
                   -dbus-linked \
                   -no-system-proxies \
                   -platform linux-clang-libc++ \
                   -prefix /opt/qt5-static

  gmake -j$NPROC
  gmake install -j$NPROC
  rm -rf qt5-build-static
)

