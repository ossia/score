#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

if [[ "$TRAVIS_BRANCH" = "$COVERITY_SCAN_BRANCH_PATTERN" ]];
then
    $CMAKE_BIN -DBOOST_ROOT="$BOOST_ROOT" -DSCORE_COTIRE:Bool=False -DSCORE_CONFIGURATION=debug ..

    eval "$COVERITY_SCAN_BUILD"
fi
