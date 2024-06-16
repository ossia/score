#!/bin/bash

export SCORE_DIR=$PWD

CXX_ADDITIONAL_FLAGS=
if c++ --version | grep clang; then 
  CXX_ADDITIONAL_FLAGS=-fexperimental-library
fi

# FIXME vst3 error in sdk hosting...
cmake -S "$SCORE_DIR" -B build \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DISABLED_PLUGINS="score-plugin-jit;score-plugin-faust" \
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj $CXX_ADDITIONAL_FLAGS" \
  -DCMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_NINJA_FORCE_RESPONSE_FILE=1 \
  -DSCORE_PCH=1

cmake --build build
cmake --build build --target install
