#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

if [[ "$TRAVIS_BRANCH" = "$COVERITY_SCAN_BRANCH_PATTERN" ]];
then
    $CMAKE_BIN -DISCORE_COTIRE:Bool=False $CMAKE_COMMON_FLAGS -DISCORE_CONFIGURATION=debug ..

    eval "$COVERITY_SCAN_BUILD"
fi
