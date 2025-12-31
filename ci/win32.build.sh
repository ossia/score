#!/bin/bash -eux

if [[ -z "${SCORE_EXTRA_CMAKE_ARGS-}" ]]; then
   export SCORE_EXTRA_CMAKE_ARGS=
fi

if [[ -z "${SCORE_CMAKE_CACHE-}" ]]; then
  export SCORE_CMAKE_CACHE_CMD=( )
else
  export SCORE_CMAKE_CACHE_CMD=(-C "$SCORE_CMAKE_CACHE")
fi

export PATH="$PATH:/c/ossia-sdk-x86_64/cmake/bin:/c/ossia-sdk-x86_64/llvm/bin"

cmake -GNinja -S "$PWD" -B build \
  "${SCORE_CMAKE_CACHE_CMD[@]}" \
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
  -DCMAKE_CXX_FLAGS="-g0" \
  $SCORE_EXTRA_CMAKE_ARGS
date
cmake --build build
date

cmake --build build --target package
date
