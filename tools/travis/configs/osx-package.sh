#!/bin/sh
source "$CONFIG_FOLDER/osx-source-qt.sh"

CMAKE_COMMON_FLAGS="-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
CMAKE_COMMON_FLAGS+=" -DCMAKE_INSTALL_PREFIX=bundle"
CMAKE_COMMON_FLAGS+=" -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk"
CMAKE_COMMON_FLAGS+=" -DOSSIA_SDK=$SCORE_SDK"
CMAKE_COMMON_FLAGS+=' -DCMAKE_C_FLAGS="-march=ivybridge -mtune=haswell"'
CMAKE_COMMON_FLAGS+=' -DCMAKE_CXX_FLAGS="-march=ivybridge -mtune=haswell"'
CMAKE_COMMON_FLAGS+=' -DCMAKE_C_COMPILER=/usr/bin/clang'
CMAKE_COMMON_FLAGS+=' -DCMAKE_CXX_COMPILER=/usr/bin/clang++'
CMAKE_COMMON_FLAGS+=' -DDEPLOYMENT_BUILD=1'
CMAKE_COMMON_FLAGS+=' -DCMAKE_INSTALL_MESSAGE=NEVER'
CMAKE_COMMON_FLAGS+=' -DSCORE_INSTALL_HEADERS=ON'
CMAKE_COMMON_FLAGS+=' -DOSSIA_STATIC_EXPORT=ON'

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
echo "SDK : $SDK_DIR"

eval "/usr/local/bin/cninja static-release linkerwarnings=no era=10.14 -- $CMAKE_COMMON_FLAGS"
(
cd build-*
xcrun $CMAKE_BIN --install . --strip --component OssiaScore
xcrun $CMAKE_BIN --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)
mv build-*/bundle .

./src/plugins/score-plugin-jit/tools/create-sdk-mac.sh

(cd SDK; zip -r -q -9 ../bundle/mac-sdk.zip usr)