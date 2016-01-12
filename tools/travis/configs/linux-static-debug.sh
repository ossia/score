#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN $CMAKE_COMMON_FLAGS -DISCORE_CONFIGURATION=static-debug ..
$CMAKE_BIN --build .
