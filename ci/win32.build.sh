#!/bin/bash -eux

export PATH="$PATH:/c/ossia-sdk-x86_64/cmake/bin:/c/ossia-sdk-x86_64/llvm/bin"
date
cmake -GNinja -S "$PWD" -B build \
  -DCMAKE_C_COMPILER=c:/ossia-sdk-x86_64/llvm/bin/clang.exe \
  -DCMAKE_CXX_COMPILER=c:/ossia-sdk-x86_64/llvm/bin/clang++.exe \
  -DOSSIA_SDK=c:/ossia-sdk-x86_64 \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_UNITY_BUILD=1 \
  -DOSSIA_STATIC_EXPORT=1 \
  -DSCORE_INSTALL_HEADERS=1 \
  -DKFR_ARCH=avx2 \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DCMAKE_C_FLAGS="-g0" \
  -DCMAKE_CXX_FLAGS="-g0"
date
cmake --build build
date

cmake --build build --target package
date
