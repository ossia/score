#!/bin/bash -eux

export PATH="$PATH:/c/ossia-sdk/cmake/bin:/c/ossia-sdk/llvm/bin"
date /t
cmake -GNinja -S "$PWD" -B build \
  -DCMAKE_C_COMPILER=c:/ossia-sdk/llvm/bin/clang.exe \
  -DCMAKE_CXX_COMPILER=c:/ossia-sdk/llvm/bin/clang++.exe \
  -DOSSIA_SDK=c:/ossia-sdk \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_UNITY_BUILD=1 \
  -DOSSIA_STATIC_EXPORT=1 \
  -DSCORE_INSTALL_HEADERS=1 \
  -DKFR_ARCH=sse2 \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DCMAKE_C_FLAGS="-g0" \
  -DCMAKE_CXX_FLAGS="-g0"
date /t
cmake --build build
date /t

cmake --build build --target package
date /t
