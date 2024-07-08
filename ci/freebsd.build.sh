#!/usr/bin/env bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_DISABLED_PLUGINS=score-plugin-vst3 \
  -DCMAKE_CXX_FLAGS="-fexperimental-library" \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install
