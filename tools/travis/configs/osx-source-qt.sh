#!/bin/sh

export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
export CXX=clang++
export CMAKE_COMMON_FLAGS="$CMAKE_COMMON_FLAGS -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH=\"$QT_PATH;$(pwd)/../../Jamoma/share/cmake\" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle"
