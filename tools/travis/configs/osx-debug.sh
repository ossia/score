#!/bin/sh
./osx-source-qt.sh

$CMAKE_BIN $CMAKE_COMMON_FLAGS  -DISCORE_CONFIGURATION=debug ..
$CMAKE_BIN --build .
