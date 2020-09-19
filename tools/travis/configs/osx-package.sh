#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

CMAKE_COMMON_FLAGS="-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
CMAKE_COMMON_FLAGS+=" -DCMAKE_INSTALL_PREFIX=bundle"
CMAKE_COMMON_FLAGS+=" -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk"
CMAKE_COMMON_FLAGS+=" -DOSSIA_SDK=$SCORE_SDK"
CMAKE_COMMON_FLAGS+=' -DCMAKE_C_FLAGS="-march=ivybridge -mtune=haswell"'
CMAKE_COMMON_FLAGS+=' -DCMAKE_CXX_FLAGS="-march=ivybridge -mtune=haswell"'
CMAKE_COMMON_FLAGS+=' -DDEPLOYMENT_BUILD=1'

eval "/usr/local/bin/cninja static-release linkerwarnings=no era=10.14 -- $CMAKE_COMMON_FLAGS"
(
cd build-*
xcrun $CMAKE_BIN --build . --target install/strip
)
mv build-*/bundle .

