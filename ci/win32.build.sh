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

# $SDK/llvm/bin goes FIRST because the Qt host tools in qt6-static/bin ship
# no DLLs of their own and import libc++.dll by name. On the ARM64 runner,
# Git for Windows is a CLANGARM64 build and puts its own older libc++.dll
# earlier on PATH, so rcc.exe resolved against that one and died with
# STATUS_ENTRYPOINT_NOT_FOUND (0xc0000139) on a libc++ 23 symbol, before
# AUTORCC could run. moc.exe does not import that symbol, which is why
# AUTOMOC passed and AUTORCC was the first thing to fail.
export PATH="$SDK/llvm/bin:$PATH:$SDK/cmake/bin"

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
