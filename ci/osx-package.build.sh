#!/bin/bash -eux
export OSSIA_SDK=/opt/ossia-sdk-$MACOS_ARCH

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
export PATH=/usr/local/bin:/opt/homebrew/bin:$PATH

if [[ -d /Applications/Xcode_15.3.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_15.3.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_15.2.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_15.2.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_15.1.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_15.1.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_15.0.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_15.0.app
  sudo xcode-select -s "$XCODE_ROOT"
else
  export XCODE_ROOT=/Applications/Xcode.app
fi

if [[ "$MACOS_ARCH" = "x86_64" ]]; then
  export CNINJA_TOOLCHAIN=macos-release-10.15
else
  export CNINJA_TOOLCHAIN=macos-release-11.0
fi

xcrun /usr/local/bin/cninja -S "$PWD" -B build "$CNINJA_TOOLCHAIN" -- \
  -DOSSIA_SDK="$OSSIA_SDK" \
  -DCMAKE_INSTALL_PREFIX="$PWD/install" \
  -DCMAKE_UNITY_BUILD=1

find . -type f -name 'ossia score' \
  | grep '.' \
  || exit 1

xcrun cmake --install build --strip --component OssiaScore
xcrun cmake --install build --strip --component Devel --prefix "$SDK_DIR/usr"

ls "$SDK_DIR/usr"

(
  ./ci/create-sdk-mac.sh
)

(
  cd SDK
  zip -r -q -9 ../install/mac-sdk.zip usr
)
