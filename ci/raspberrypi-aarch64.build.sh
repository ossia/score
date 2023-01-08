#!/bin/bash -eux

export SCORE_DIR=$PWD

mkdir -p /build || true
chown -R $(whoami) /build
cd /build

cmake $SCORE_DIR \
 -GNinja \
 -DCMAKE_UNITY_BUILD=1 \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_TOOLCHAIN_FILE=/opt/ossia-sdk-rpi-aarch64/toolchain.aarch64.llvm.cmake \
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
  cp /opt/ossia-sdk-rpi-aarch64/aarch64-rpi3-linux-gnu/aarch64-rpi3-linux-gnu/lib/libstdc++.so.6 lib/
  cp $SCORE_DIR/cmake/Deployment/Linux/Raspberry/* .
)

export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")
mv install "ossia score-$TAG"
tar caf "$SCORE_DIR/score.tar.gz" "ossia score-$TAG"
