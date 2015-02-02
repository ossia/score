#!/bin/bash -xue

if [[ "$OSTYPE" == "darwin"* ]]; then


brew install qt5 boost 
mkdir -p build
cd build

ISCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt5 -name Qt5Config.cmake)"
ISCORE_CMAKE_QT_PATH="$(dirname $(dirname $ISCORE_CMAKE_QT_CONFIG))"
cmake -DCMAKE_PREFIX_PATH="$ISCORE_CMAKE_QT_PATH/Qt5;$ISCORE_CMAKE_QT_PATH/Qt5Widgets;$ISCORE_CMAKE_QT_PATH/Qt5Network;$ISCORE_CMAKE_QT_PATH/Qt5Test;$ISCORE_CMAKE_QT_PATH/Qt5Gui;$ISCORE_CMAKE_QT_PATH/Qt5Xml;$ISCORE_CMAKE_QT_PATH/Qt5Core;" ..
make -j4

else
	echo "Only for OS X"
fi
