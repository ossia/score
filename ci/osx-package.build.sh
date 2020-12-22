#!/bin/bash
export OSSIA_SDK=/opt/ossia-sdk

export CMAKE_BIN=$(find $OSSIA_SDK -type f -perm +111 -name cmake)
if [[ "x$CMAKE_BIN" == "x" ]]; then
  export CMAKE_BIN=$(find /usr/local/bin -name cmake)
fi


CMAKE_COMMON_FLAGS="-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
CMAKE_COMMON_FLAGS+=" "

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
echo "SDK : $SDK_DIR"

xcrun /usr/local/bin/cninja macos-release -- -DOSSIA_SDK=$OSSIA_SDK
(
cd build-*
xcrun $CMAKE_BIN --install . --strip --component OssiaScore
xcrun $CMAKE_BIN --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)
mv build-*/install ./install

./ci/create-sdk-mac.sh

(
  cd SDK
  zip -r -q -9 ../install/mac-sdk.zip usr
)