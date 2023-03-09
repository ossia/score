#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

if command -v gcc-13; then
  export CC=gcc-13
  export CXX=g++-13
elif command -v gcc-12; then
  export CC=gcc-12
  export CXX=g++-12
elif command -v gcc-11; then
  export CC=gcc-11
  export CXX=g++-11
fi

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install