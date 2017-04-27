#!/bin/sh

export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
export CXX=clang++
export CMAKE_PREFIX_PATH="$QT_PATH"
export CMAKE_COMMON_FLAGS="-DBOOST_ROOT=/usr/local/Cellar/boost/1.64.0 -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 -DOSSIA_DISABLE_COTIRE=1"
