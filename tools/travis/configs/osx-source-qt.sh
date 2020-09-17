#!/bin/bash
export SCORE_SDK=/opt/score-sdk-osx
if [[ -d "$SCORE_SDK/qt5-static" ]]; then
  export QT_PATH=$SCORE_SDK/qt5-static/lib/cmake/Qt5
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
CMAKE_COMMON_FLAGS="-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
CMAKE_COMMON_FLAGS+=" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle"
CMAKE_COMMON_FLAGS+=" -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14"
CMAKE_COMMON_FLAGS+=" -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk"
CMAKE_COMMON_FLAGS+=" -DOSSIA_SDK=$SCORE_SDK"
CMAKE_COMMON_FLAGS+=' -DCMAKE_C_FLAGS="-march=ivybridge -mtune=haswell"'
CMAKE_COMMON_FLAGS+=' -DCMAKE_CXX_FLAGS="-march=ivybridge -mtune=haswell"'
export CMAKE_COMMON_FLAGS