#!/bin/bash
if [[ -d "/opt/score-sdk/qt5-dynamic" ]]; then
  export QT_PATH=/opt/score-sdk/qt5-dynamic/lib/cmake/Qt5
elif [[ -d "/usr/local/Cellar/qt" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt -name Qt5Config.cmake) ) )
elif [[ -d "/usr/local/Cellar/qt5" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
fi

export CMAKE_BIN=$(find /opt/score-sdk -type f -perm +111 -name cmake)
if [[ "x$CMAKE_BIN" == "x" ]]; then
  export CMAKE_BIN=$(find /usr/local/bin -name cmake)
fi
export CC=clang
export CXX=clang++
export CMAKE_PREFIX_PATH="$QT_PATH"
export CMAKE_COMMON_FLAGS="-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle -DCMAKE_OSX_DEPLOYMENT_TARGET=10.12"

if [[ -d "/opt/ossia-sdk" ]]; then
  export CMAKE_COMMON_FLAGS="$CMAKE_COMMON_FLAGS -DOSSIA_SDK=/opt/ossia-sdk"
elif [[ -d "/opt/score-sdk" ]]; then
  export CMAKE_COMMON_FLAGS="$CMAKE_COMMON_FLAGS -DOSSIA_SDK=/opt/score-sdk"
elif [[ -d "/opt/score-sdk-osx" ]]; then
  export CMAKE_COMMON_FLAGS="$CMAKE_COMMON_FLAGS -DOSSIA_SDK=/opt/score-sdk-osx"
fi
