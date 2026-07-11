#!/bin/bash -eux

if [[ -z "${SCORE_EXTRA_CMAKE_ARGS-}" ]]; then
   export SCORE_EXTRA_CMAKE_ARGS=
fi

if [[ -z "${SCORE_CMAKE_CACHE-}" ]]; then
  export SCORE_CMAKE_CACHE_CMD=( )
else
  export SCORE_CMAKE_CACHE_CMD=(-C "$SCORE_CMAKE_CACHE")
fi

# Pick the SDK + kfr arch matching the runner architecture.
if [[ "${RUNNER_ARCH:-X64}" == "ARM64" ]]; then
  SDK_ARCH=aarch64
  KFR_ARCH=neon
else
  SDK_ARCH=x86_64
  KFR_ARCH=avx2
fi
SDK=/c/ossia-sdk-$SDK_ARCH
SDK_CMAKE=c:/ossia-sdk-$SDK_ARCH

export PATH="$PATH:$SDK/cmake/bin:$SDK/llvm/bin"

cmake -GNinja -S "$PWD" -B build \
  "${SCORE_CMAKE_CACHE_CMD[@]}" \
  -DCMAKE_C_COMPILER=$SDK_CMAKE/llvm/bin/clang.exe \
  -DCMAKE_CXX_COMPILER=$SDK_CMAKE/llvm/bin/clang++.exe \
  -DOSSIA_SDK=$SDK_CMAKE \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_UNITY_BUILD=1 \
  -DOSSIA_STATIC_EXPORT=1 \
  -DSCORE_INSTALL_HEADERS=1 \
  -DKFR_ARCH=$KFR_ARCH \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DCMAKE_C_FLAGS="-g0" \
  -DCMAKE_CXX_FLAGS="-g0" \
  $SCORE_EXTRA_CMAKE_ARGS
date
cmake --build build
date

cmake --build build --target package
date
