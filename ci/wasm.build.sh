#!/bin/bash -eux

export SCORE_DIR=$PWD

# /build is shared with wasm.deploy.sh. On the ubuntu-24.04 runner (no container)
# the script runs as non-root, so create it via sudo instead of plain mkdir.
sudo mkdir -p /build
sudo chown -R "$(whoami)" /build
cd /build

source /opt/ossia-sdk-wasm/emsdk/emsdk_env.sh

/opt/ossia-sdk-wasm/qt-wasm/bin/qt-cmake  $SCORE_DIR \
   -GNinja \
   -DCMAKE_CXX_STANDARD=23 \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_UNITY_BUILD=1 \
   -DCPU_ARCH=sse41 \
   -DKFR_ARCH=sse41 \
   -DOSSIA_PCH=0 \
   -DSCORE_PCH=0 \
   -DCMAKE_CXX_SCAN_FOR_MODULES=0 \
   -DOSSIA_SDK=/opt/ossia-sdk-wasm/ \
   -DCMAKE_C_FLAGS='-pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -flto -g0 ' \
   -DCMAKE_CXX_FLAGS='-fexperimental-library -DBOOST_ASIO_DISABLE_EPOLL=1 -pthread -O3 -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -flto -g0 '  \
   -DCMAKE_EXE_LINKER_FLAGS='-sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=268435456 -sMAXIMUM_MEMORY=2147483648 -pthread -Oz -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -flto -g0 ' \
   -DCMAKE_SHARED_LINKER_FLAGS='-sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -pthread -Oz -ffast-math -msimd128 -msse -msse2 -msse3 -mssse3 -msse4 -msse4.1 -msse4.2 -flto -g0 '

cmake --build .

