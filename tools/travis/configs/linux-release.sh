#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -GNinja \
           -DISCORE_COTIRE_DISABLE_UNITY:Bool=True \
           -DISCORE_CONFIGURATION=release ..

$CMAKE_BIN --build . --target package
