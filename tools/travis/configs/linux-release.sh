#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DISCORE_CONFIGURATION=release ..

$CMAKE_BIN --build . --target all_unity
$CMAKE_BIN --build . --target package
