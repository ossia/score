#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj" \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install