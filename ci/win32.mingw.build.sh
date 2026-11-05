#!/bin/bash -e

export SCORE_DIR=$PWD

CXX_ADDITIONAL_FLAGS=
# GCC needs several GB for each of the larger avnd translation units: at
# Ninja's default of cores + 2 jobs, three of them at once exhaust the 16 GB of
# the windows-latest runners (cc1plus: out of memory).
COMPILE_JOBS=$(nproc)
if c++ --version | grep clang; then 
  CXX_ADDITIONAL_FLAGS=-fexperimental-library
  COMPILE_JOBS=
fi

cmake -S "$SCORE_DIR" -B build \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DISABLED_PLUGINS="score-plugin-jit;score-plugin-faust" \
  -DCMAKE_CXX_FLAGS="-Wa,-mbig-obj $CXX_ADDITIONAL_FLAGS" \
  -DCMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS=1 \
  -DCMAKE_NINJA_FORCE_RESPONSE_FILE=1 \
  -DSCORE_DISABLE_SNMALLOC=1 \
  -DSCORE_PCH=1 \
  -DSCORE_COMPILE_JOBS="$COMPILE_JOBS" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=1

cmake --build build
cmake --build build --target install
