#!/bin/sh
./osx-source-qt.sh

$CMAKE_BIN $CMAKE_COMMON_FLAGS  -DISCORE_CONFIGURATION=static-release ..
$CMAKE_BIN --build . --target install/strip --config StaticRelease
