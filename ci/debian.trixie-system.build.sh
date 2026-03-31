#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_C_COMPILER=clang-19 \
  -DCMAKE_CXX_COMPILER=clang++-19 \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DSCORE_STATIC_PLUGINS=1 \
  -DSCORE_FHS_BUILD=1 \
  -DSCORE_USE_SYSTEM_LIBRARIES=1 \
  -DBUILD_SHARED_LIBS=0 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_UNITY_BUILD=1 \
  -DCMAKE_CXX_FLAGS="-g0 -Wno-psabi" \
  -DCMAKE_C_FLAGS="-g0" \
  $CMAKEFLAGS

cmake --build .
cmake --build . --target install
cmake --build . --target package

cp *.deb "$SCORE_DIR/" || true
