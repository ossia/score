#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

eval "xcrun $CMAKE_BIN $CMAKE_COMMON_FLAGS -DCMAKE_UNITY_BUILD=1 -DSCORE_CONFIGURATION=static-debug .."
xcrun $CMAKE_BIN --build . -- -j2
