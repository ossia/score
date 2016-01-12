#!/bin/sh
./osx-source-qt.sh

$CMAKE_BIN $CMAKE_COMMON_FLAGS  -DISCORE_CONFIGURATION=release ..
$CMAKE_BIN --build . --target install/strip --config DynamicRelease
