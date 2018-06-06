#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DBOOST_ROOT="$BOOST_ROOT" -DSCORE_CONFIGURATION=static-debug-travis ..
$CMAKE_BIN --build . --target all_unity -- -j2
