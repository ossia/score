#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

$CMAKE_BIN -DISCORE_CONFIGURATION=static-debug ..
$CMAKE_BIN --build . --target all_unity
