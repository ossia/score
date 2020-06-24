#!/bin/bash -xue

# To make a dev build, just run this script
# To make a release (optimized) build, run `./build.sh release`

BUILD_DIR=build
BUILD_TYPE=developer
INSTALL=0
if [[ -n "${1+x}" ]];
then
    case "$1" in
     "release" )
         BUILD_DIR=build-release
         BUILD_TYPE=static-release
         INSTALL=1
    ;;
    esac
fi

mkdir -p $BUILD_DIR
cd $BUILD_DIR

if [[ "$OSTYPE" == "darwin"* ]]; then

brew install qt5 boost ninja ffmpeg tbb jack || true

SCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt -name Qt5Config.cmake | head -1)"
VAR1=`dirname $SCORE_CMAKE_QT_CONFIG`
SCORE_CMAKE_QT_PATH=`dirname $VAR1`

if [[ -f CMakeCache.txt ]]; then
CMAKE_FOLDER=$PWD
else
CMAKE_FOLDER=$PWD/..
fi

cmake \
      -Wno-dev \
      -DCMAKE_PREFIX_PATH="$SCORE_CMAKE_QT_PATH/Qt5;$SCORE_CMAKE_QT_PATH/Qt5Widgets;$SCORE_CMAKE_QT_PATH/Qt5Network;$SCORE_CMAKE_QT_PATH/Qt5Gui;$SCORE_CMAKE_QT_PATH/Qt5Xml;$SCORE_CMAKE_QT_PATH/Qt5Core" \
      -DSCORE_CONFIGURATION=$BUILD_TYPE \
      -DCMAKE_INSTALL_PREFIX=build/ \
      -DCMAKE_UNITY_BUILD=1 \
      -GNinja \
      "$CMAKE_FOLDER"

cmake --build . -- -j4

if [[ "$INSTALL" == "1" ]];
then
    cmake --build . --target install -- -j4
    rm -rf score.app
    mv build/score.app .
fi

else
	# assume all dev packages are already installed
	cmake \
        -Wno-dev \
        -DSCORE_CONFIGURATION=$BUILD_TYPE \
        -DPORTAUDIO_ONLY_DYNAMIC=1 \
        -DDEPLOYMENT_BUILD=1 \
        -DCMAKE_SKIP_RPATH=ON \
        ..
	cmake --build . -- -j$(nproc)

	echo "OK - you should have a ossia-score executable in the $BUILD_DIR folder"
fi
