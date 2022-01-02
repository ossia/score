#!/bin/bash

mkdir build
cd build

cmake .. \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DCMAKE_UNITY_BUILD=1 \
  -DCMAKE_EXE_LINKER_FLAGS="-lstdc++fs" # Needed for VST3

cmake --build .
cmake --build . --target install