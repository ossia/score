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
  -DSCORE_DISABLE_SNMALLOC=1 \
  -DSCORE_PCH=1 \
  -DFETCHCONTENT_FULLY_DISCONNECTED=1

cmake --build build
cmake --build build --target install
d:\dev\cmake\bin\cmake.exe -S score -B build-clang-cl -GNinja -DCMAKE_BUILD_TYPE=Release ^
   -DCMAKE_C_COMPILER=clang-cl ^
   -DCMAKE_CXX_COMPILER=clang-cl ^
   -DOSSIA_SDK=c:/ossia-sdk-msvc ^
   -DCMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS=1 ^
  -DCMAKE_NINJA_FORCE_RESPONSE_FILE=1 ^
  -DSCORE_DISABLE_SNMALLOC=1 ^
  -DSCORE_PCH=1 ^
  -DFETCHCONTENT_FULLY_DISCONNECTED=1 ^
  -DCMAKE_C_FLAGS=" -D_WIN32_WINNT_=0x0A00 -DWINVER=0x0A00 " ^
  -DCMAKE_CXX_FLAGS=" -D_WIN32_WINNT_=0x0A00 -DWINVER=0x0A00 " ^
  -DCMAKE_PREFIX_PATH=D:/6.10.1/msvc2022_64