#!/bin/bash
export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/build/SDK"

# Copy windows binary
(
  cd build
  dir
  move "ossia score-3.0.0-win64.exe" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$GITTAGNOV-win64.exe"
)

# Create SDK files
(
  cd build
  cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)

./src/plugins/score-plugin-jit/tools/create-sdk-mingw.sh

# Copy SDK
(
    cd build/SDK
    7z a "$BUILD_ARTIFACTSTAGINGDIRECTORY/windows-sdk.zip" usr
)