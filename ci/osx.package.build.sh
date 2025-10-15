#!/bin/bash -eux
export OSSIA_SDK=/opt/ossia-sdk-$MACOS_ARCH

export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/SDK"
export PATH=/usr/local/bin:/opt/homebrew/bin:$PATH

if [[ -d /Applications/Xcode_16.4.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.4.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_16.3.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.3.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_16.2.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.2.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_16.1.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.1.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_16.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.app
  sudo xcode-select -s "$XCODE_ROOT"
elif [[ -d /Applications/Xcode_16.0.app ]]; then
  export XCODE_ROOT=/Applications/Xcode_16.0.app
  sudo xcode-select -s "$XCODE_ROOT"
else
  export XCODE_ROOT=/Applications/Xcode.app
fi

if [[ "$MACOS_ARCH" = "x86_64" ]]; then
  export CMAKE_OSX_ARCH=x86_64
else
  export CMAKE_OSX_ARCH=arm64
fi

if [[ -z "${SCORE_EXTRA_CMAKE_ARGS-}" ]]; then
   export SCORE_EXTRA_CMAKE_ARGS=
fi


xcrun /usr/local/bin/cninja -S "$PWD" -B build \
  macos-release-12.0 -- \
  -DOSSIA_SDK="$OSSIA_SDK" \
  -DCMAKE_INSTALL_PREFIX="$PWD/install" \
  -DCMAKE_UNITY_BUILD=1 \
  -DCMAKE_OSX_ARCHITECTURES="$CMAKE_OSX_ARCH" \
  $SCORE_EXTRA_CMAKE_ARGS


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
