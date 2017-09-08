#!/bin/sh
export CC=gcc-6
export CXX=g++-6

$CMAKE_BIN -DBOOST_ROOT="$BOOST_ROOT" -DCMAKE_PREFIX_PATH="/opt/qt5-static/lib/cmake/Qt5" -DSCORE_CONFIGURATION=staticqt-release $CMAKE_COMMON_FLAGS ..

$CMAKE_BIN --build . --target all_unity -- -j2
$CMAKE_BIN --build . --target package -- -j2
