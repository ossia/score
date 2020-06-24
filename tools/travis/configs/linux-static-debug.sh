#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DSCORE_CONFIGURATION=static-debug-travis -DCMAKE_UNITY_BUILD=1 ..
$CMAKE_BIN --build . -- -j2
