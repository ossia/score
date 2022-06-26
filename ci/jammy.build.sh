#!/bin/bash

mkdir build
cd build

export CC=gcc-11
export CXX=g++-11

cmake .. \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_UNITY_BUILD=1

cmake --build .
cmake --build . --target install