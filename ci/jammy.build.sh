#!/bin/bash

mkdir build
cd build

cmake .. \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_UNITY_BUILD=1

cmake --build .
cmake --build . --target install