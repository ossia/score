#!/bin/bash -xue

if [[ "$OSTYPE" == "darwin"* ]]; then

BUILD_DIR=build
BUILD_TYPE=developer

case "$1" in
 "release" ) 
     BUILD_DIR=build-release
     BUILD_TYPE=release
 ;;
esac

brew install qt5 boost 
mkdir -p $BUILD_DIR
cd $BUILD_DIR

ISCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt5 -name Qt5Config.cmake | head -1)"
VAR1=`dirname $ISCORE_CMAKE_QT_CONFIG`
ISCORE_CMAKE_QT_PATH=`dirname $VAR1`

if [[ -f CMakeCache.txt ]]
CMAKE_FOLDER=$PWD
else
CMAKE_FOLDER=$PWD/..
fi

cmake -DCMAKE_PREFIX_PATH="$ISCORE_CMAKE_QT_PATH/Qt5;$ISCORE_CMAKE_QT_PATH/Qt5Widgets;$ISCORE_CMAKE_QT_PATH/Qt5Network;$ISCORE_CMAKE_QT_PATH/Qt5Test;$ISCORE_CMAKE_QT_PATH/Qt5Gui;$ISCORE_CMAKE_QT_PATH/Qt5Xml;$ISCORE_CMAKE_QT_PATH/Qt5Core;/usr/local/jamoma/share/cmake/Jamoma" \
      -DISCORE_CONFIGURATION=$BUILD_TYPE $CMAKE_FOLDER
      
cmake --build . -- -j4

else
    echo "Only for OS X"
fi
