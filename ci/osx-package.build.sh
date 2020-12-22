#!/bin/bash
export OSSIA_SDK=/opt/ossia-sdk

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
export PATH=/usr/local/bin:$PATH
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer

xcrun env
xcrun ninja


xcrun /usr/local/bin/cninja macos-release -- -DOSSIA_SDK=$OSSIA_SDK
(
cd build-*
xcrun cmake --install . --strip --component OssiaScore
xcrun cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)
mv build-*/install ./install

./ci/create-sdk-mac.sh

(
  cd SDK
  zip -r -q -9 ../install/mac-sdk.zip usr
)