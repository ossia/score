#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_PCH=1 \
  -DCMAKE_CXX_SCAN_FOR_MODULES=0

cmake --build .
cmake --build . --target install
