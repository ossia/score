#!/bin/bash -eux
export OSSIA_SDK=/opt/ossia-sdk-$MACOS_ARCH

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
export PATH=/usr/local/bin:$PATH
export XCODE_ROOT=/Applications/Xcode_15.0.app
sudo xcode-select -s $XCODE_ROOT

xcrun /usr/local/bin/cninja macos-release-$MACOS_ARCH -- \
  -DOSSIA_SDK="$OSSIA_SDK" \
  -DCMAKE_INSTALL_PREFIX="$PWD/install"

find . -type f -name 'ossia score' \
  | grep '.' \
  || exit 1

(
  cd build-*
  echo "Installing OssiaScore: "
  xcrun cmake --install . --strip --component OssiaScore
)


(
  cd build-*
  echo "Installing Devel: "
  xcrun cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)

ls "$SDK_DIR/usr"

(
  echo "./ci/create-sdk-mac.sh"
  ./ci/create-sdk-mac.sh
)

(
  cd SDK
  zip -r -q -9 ../install/mac-sdk.zip usr
)
