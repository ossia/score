#!/bin/bash

export SCORE_DIR=$PWD

mkdir -p build || true
cd build

# FIXME vst3 error in sdk hosting...

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DISABLED_PLUGINS="score-plugin-vst3;score-plugin-jit;score-plugin-faust" \
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj" \
  -DCMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_NINJA_FORCE_RESPONSE_FILE=1 \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install