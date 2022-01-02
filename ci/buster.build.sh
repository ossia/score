#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

# Debian Buster does not support building KFR

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DOSSIA_DISABLE_KFR=1 \
  -DOSSIA_FFT_FFTW=1 \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install
