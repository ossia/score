#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DSCORE_CONFIGURATION=release $CMAKE_COMMON_FLAGS -DCMAKE_SKIP_RPATH=ON ..

$(dirname $(which cmake))/cpack --config CPackSourceConfig.cmake -G TXZ
