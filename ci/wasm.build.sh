#!/bin/bash -eux

export SCORE_DIR=$PWD

mkdir -p /build || true
chown -R $(whoami) /build
cd /build

source /opt/ossia-sdk-wasm/emsdk/emsdk_env.sh

/opt/ossia-sdk-wasm/qt-wasm/bin/qt-cmake  $SCORE_DIR \
   -GNinja \
   -DCMAKE_CXX_STANDARD=20 \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_UNITY_BUILD=1 \
   -DCPU_ARCH=avx \
   -DQT_VERSION="Qt6;6.4" \
   -DOSSIA_PCH=0 \
   -DSCORE_PCH=0 \
   -DCMAKE_C_FLAGS='-pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx ' \
   -DCMAKE_CXX_FLAGS='-pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx  '  \
   -DCMAKE_EXE_LINKER_FLAGS='-sASYNCIFY -pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx  ' \
   -DCMAKE_SHARED_LINKER_FLAGS='-sASYNCIFY -pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -mavx '

cmake --build .

