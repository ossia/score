#!/bin/bash

install_name_tool -add_rpath @executable_path/../Frameworks "$1/MacOS/score"
install_name_tool -add_rpath @executable_path/../Frameworks "$1/MacOS/ossia-score-vstpuppet"

# Also fixup for the QtQuick dylibs
find "$1" -name '*dSYM' -exec rm -rf {} \;
find "$1" -name '*_debug*' -exec rm -rf {} \;
find "$1" -name '*qmlc' -exec rm -rf {} \;
