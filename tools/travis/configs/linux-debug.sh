#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DSCORE_CONFIGURATION=debug -DCMAKE_UNITY_BUILD=1 $CMAKE_COMMON_FLAGS ..
$CMAKE_BIN --build . -- -j2
