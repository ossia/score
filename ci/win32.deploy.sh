#!/bin/bash
export SCORE_DIR="$PWD"
export SDK_DIR="$PWD/build/SDK"
ls
(
  cd build
  cmake --install . --strip --component Devel --prefix "$SDK_DIR/usr"
)

./src/plugins/score-plugin-jit/tools/create-sdk-mingw.sh

(
    cd build/SDK
    7z a "$BUILD_ARTIFACTSTAGINGDIRECTORY/windows-sdk.zip" usr
)