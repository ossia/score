#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

export CC=gcc-10
export CXX=g++-10

mkdir build && cd build

$CMAKE_BIN -DCMAKE_PREFIX_PATH="/opt/qt5-static/lib/cmake/Qt5" -DCMAKE_BUILD_TYPE=Release -DCMAKE_UNITY_BUILD=1 $CMAKE_COMMON_FLAGS ..

$CMAKE_BIN --build . -- -j2
$CMAKE_BIN --build . --target package -- -j2
