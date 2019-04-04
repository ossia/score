#!/bin/bash
if [[ -d "/opt/score-sdk/qt5-dynamic" ]]; then
  export QT_PATH=/opt/score-sdk/qt5-dynamic/lib/cmake/Qt5
elif [[ -d "/usr/local/Cellar/qt" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt -name Qt5Config.cmake) ) )
elif [[ -d "/usr/local/Cellar/qt5" ]]; then
  export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
fi

export CMAKE_BIN=$(find /opt/score-sdk -type f -perm +111 -name cmake)
export CC=clang
export CXX=clang++
export CMAKE_PREFIX_PATH="$QT_PATH"
export CMAKE_COMMON_FLAGS="-DBOOST_ROOT=/opt/score-sdk/boost -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle -DCMAKE_OSX_DEPLOYMENT_TARGET=10.12 -DOSSIA_SDK=/opt/score-sdk"
