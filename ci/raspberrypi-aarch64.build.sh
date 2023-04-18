#!/bin/bash -eux

export SCORE_DIR=$PWD

mkdir -p /build || true
chown -R $(whoami) /build
cd /build

echo '#include <thread>
#include <cstdio>

int main() { 
  std::thread t{ [] { printf("hello"); } };
  t.join();
}' > thread.cpp
echo "add_executable(thread thread.cpp)" > CMakeLists.txt
mkdir bb
cd bb
clang++ -v -std=c++20 ../thread.cpp

cmake .. \
 -GNinja \
 -DQT_VERSION=Qt6 \
 -DCMAKE_UNITY_BUILD=1 \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_TOOLCHAIN_FILE=/opt/ossia-sdk-rpi-aarch64/toolchain.aarch64.llvm-host.cmake \
 -DCMAKE_PREFIX_PATH=/opt/ossia-sdk-rpi-aarch64/pi/sysroot/opt/ossia-sdk-rpi/qt6-static/lib/cmake \
 -DOSSIA_SDK=/opt/ossia-sdk-rpi-aarch64/pi/sysroot/opt/ossia-sdk-rpi \
 -DSCORE_DEPLOYMENT_BUILD=1 \
 -DOSSIA_ENABLE_KFR=1 \
 -DOSSIA_ENABLE_FFTW=0 \
 -DCMAKE_INSTALL_PREFIX=install || true

ls CMakeFiles
echo " ============= "
cat  CMakeFiles/CMakeConfigureLog.yaml  || true

echo " ============= "
cat  CMakeFiles/*.log  || true
cmake --build .


cmake $SCORE_DIR \
 -GNinja \
 -DQT_VERSION=Qt6 \
 -DCMAKE_UNITY_BUILD=1 \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_TOOLCHAIN_FILE=/opt/ossia-sdk-rpi-aarch64/toolchain.aarch64.llvm-host.cmake \
 -DCMAKE_PREFIX_PATH=/opt/ossia-sdk-rpi-aarch64/pi/sysroot/opt/ossia-sdk-rpi/qt6-static/lib/cmake \
 -DOSSIA_SDK=/opt/ossia-sdk-rpi-aarch64/pi/sysroot/opt/ossia-sdk-rpi \
 -DSCORE_DEPLOYMENT_BUILD=1 \
 -DOSSIA_ENABLE_KFR=1 \
 -DOSSIA_ENABLE_FFTW=0 \
 -DCMAKE_INSTALL_PREFIX=install \
 -DSCORE_LINKER_SCRIPT="$SCORE_DIR/cmake/Deployment/Linux/AppImage/version"

cmake --build .
cmake --build . --target install/strip

(
  cd install
  rm -rf include lib share/doc
  mkdir lib
  cp /opt/ossia-sdk-rpi-aarch64/aarch64-rpi3-linux-gnu/aarch64-rpi3-linux-gnu/lib64/libstdc++.so.6 lib/
  cp $SCORE_DIR/cmake/Deployment/Linux/Raspberry/* .
)

export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")
mv install "ossia score-$TAG"
tar caf "$SCORE_DIR/score.tar.gz" "ossia score-$TAG"
