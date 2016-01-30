#!/bin/sh
export CC=gcc-5
export CXX=g++-5

$CMAKE_BIN -DCMAKE_PREFIX_PATH="/usr/local/jamoma/share/cmake/Jamoma;/opt/qt5-static/lib/cmake/Qt5" -DISCORE_CONFIGURATION=staticqt-release ..

$CMAKE_BIN --build . --target all_unity -- -j2
$CMAKE_BIN --build . --target package -- -j2
