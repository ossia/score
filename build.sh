#!/bin/bash -xue

if [[ "$OSTYPE" == "darwin"* ]]; then

BUILD_DIR=build
BUILD_TYPE=developer
INSTALL=0
if [[ -n "${1+x}" ]];
then
    case "$1" in
     "release" )
         BUILD_DIR=build-release
         BUILD_TYPE=release
         INSTALL=1
    ;;
    esac
fi

brew install qt5 boost ninja ffmpeg tbb jack || true
mkdir -p $BUILD_DIR
cd $BUILD_DIR

SCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt -name Qt5Config.cmake | head -1)"
VAR1=`dirname $SCORE_CMAKE_QT_CONFIG`
SCORE_CMAKE_QT_PATH=`dirname $VAR1`

if [[ -f CMakeCache.txt ]]; then
CMAKE_FOLDER=$PWD
else
CMAKE_FOLDER=$PWD/..
fi

cmake -DCMAKE_PREFIX_PATH="$SCORE_CMAKE_QT_PATH/Qt5;$SCORE_CMAKE_QT_PATH/Qt5Widgets;$SCORE_CMAKE_QT_PATH/Qt5Network;$SCORE_CMAKE_QT_PATH/Qt5Test;$SCORE_CMAKE_QT_PATH/Qt5Gui;$SCORE_CMAKE_QT_PATH/Qt5Xml;$SCORE_CMAKE_QT_PATH/Qt5Core" \
      -DSCORE_CONFIGURATION=$BUILD_TYPE \
      -DCMAKE_INSTALL_PREFIX=build/ \
      -DSCORE_CONFIGURATION=static-release \
      -DSCORE_COTIRE:Bool=OFF \
      -GNinja \
      "$CMAKE_FOLDER"

cmake --build . -- -j4

if [[ "$INSTALL" == "1" ]];
then
    cmake --build . -- -j4
    cmake --build . --target install -- -j4
    rm -rf score.app
    mv build/score.app .
fi

else
    echo "Only for OS X"
fi
