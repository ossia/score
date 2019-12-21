#!/bin/bash
export SCORE_SDK=/opt/score-sdk-osx
if [[ -d "$SCORE_SDK/qt5-dynamic" ]]; then
  export QT_PATH=$SCORE_SDK/qt5-dynamic/lib/cmake/Qt5
elif [[ -d "/usr/local/Cellar/qt" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt -name Qt5Config.cmake) ) )
elif [[ -d "/usr/local/Cellar/qt5" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
fi

export CMAKE_BIN=$(find $SCORE_SDK -type f -perm +111 -name cmake)
if [[ "x$CMAKE_BIN" == "x" ]]; then
  export CMAKE_BIN=$(find /usr/local/bin -name cmake)
fi
export CC=clang
export CXX=clang++
export CMAKE_PREFIX_PATH="$QT_PATH"
export CMAKE_COMMON_FLAGS="-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle -DCMAKE_OSX_DEPLOYMENT_TARGET=10.12"

export CMAKE_COMMON_FLAGS="$CMAKE_COMMON_FLAGS -DOSSIA_SDK=$SCORE_SDK"