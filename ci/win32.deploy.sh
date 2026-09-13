#!/bin/bash -e
export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/build/SDK"

if [[ "${RUNNER_ARCH:-X64}" == "ARM64" ]]; then
  SDK_ARCH=aarch64
else
  SDK_ARCH=x86_64
fi

# Copy windows binary
(
  cd build
  ls
  mv ossia\ score-*-win64.exe "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$GITTAGNOV-$SDK_ARCH.exe"
)

# Create SDK files
(
  cd build
  cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)

./ci/create-sdk-mingw.sh

# ScoreExternalAddon.sdk.cmake compiles add-ons with -nostdinc against
# include/c++/v1, so an SDK without the toolchain headers can build nothing at all.
for required in "$SDK_DIR/usr/include/c++" "$SDK_DIR/usr/include/_mingw.h"; do
  if [[ ! -e "$required" ]]; then
    echo "error: the SDK is incomplete, '$required' is missing" >&2
    exit 1
  fi
done

# Copy SDK
(
    cd build/SDK
    7z a "$BUILD_ARTIFACTSTAGINGDIRECTORY/sdk-windows-$SDK_ARCH.zip" usr
)
