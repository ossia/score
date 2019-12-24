#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

eval "xcrun $CMAKE_BIN $CMAKE_COMMON_FLAGS -DSCORE_CONFIGURATION=release .."
xcrun $CMAKE_BIN --build . --target all_unity -- -j2
xcrun $CMAKE_BIN --build . --target install/strip/fast -- -j2
