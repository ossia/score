#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DSCORE_CONFIGURATION=debug $CMAKE_COMMON_FLAGS ..
$CMAKE_BIN --build . --target all_unity -- -j2
