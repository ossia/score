#!/bin/sh
./linux-source-qt.sh

$CMAKE_BIN $CMAKE_COMMON_FLAGS -DISCORE_CONFIGURATION=release ..

$CMAKE_BIN --build . --target package --config DynamicRelease
