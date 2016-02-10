#!/bin/bash -xue

if [[ "$OSTYPE" == "darwin"* ]]; then


brew install qt5 boost 
mkdir -p build
cd build

ISCORE_CMAKE_QT_CONFIG="$(find /usr/local/Cellar/qt5 -name Qt5Config.cmake | head -1)"
VAR1=`dirname $ISCORE_CMAKE_QT_CONFIG`
ISCORE_CMAKE_QT_PATH=`dirname $VAR1`
cmake -DCMAKE_PREFIX_PATH="$ISCORE_CMAKE_QT_PATH/Qt5;$ISCORE_CMAKE_QT_PATH/Qt5Widgets;$ISCORE_CMAKE_QT_PATH/Qt5Network;$ISCORE_CMAKE_QT_PATH/Qt5Test;$ISCORE_CMAKE_QT_PATH/Qt5Gui;$ISCORE_CMAKE_QT_PATH/Qt5Xml;$ISCORE_CMAKE_QT_PATH/Qt5Core;" ../i-score
make -j8

else
	echo "Only for OS X"
fi
