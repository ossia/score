#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

if [[ "$TRAVIS_BRANCH" = "$COVERITY_SCAN_BRANCH_PATTERN" ]];
then
    mkdir build && cd build
    $CMAKE_BIN -DSCORE_PCH:Bool=False -DCMAKE_BUILD_TYPE=Debug ..

    eval "$COVERITY_SCAN_BUILD"
fi
