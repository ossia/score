#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

$CMAKE_BIN -GNinja -DISCORE_CONFIGURATION=release ..
$CMAKE_BIN --build . --target install/strip --config DynamicRelease
