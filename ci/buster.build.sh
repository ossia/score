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
  -DOSSIA_ENABLE_KFR=0 \
  -DOSSIA_ENABLE_FFTW=1 \
  -DSCORE_PCH=1 \
  -DQT_VERSION=Qt5 \
  -DSCORE_DISABLED_PLUGINS=score-plugin-vst3

cmake --build .
cmake --build . --target install
