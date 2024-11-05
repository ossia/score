#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_C_COMPILER=clang-19 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_USE_SYSTEM_LIBRARIES=1 \
  -DCMAKE_CXX_FLAGS="-Wno-psabi" \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install
# cmake --build . --target package

mv *.deb ..
