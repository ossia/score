#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DISCORE_CONFIGURATION=static-debug ..
$CMAKE_BIN --build . --target all_unity
