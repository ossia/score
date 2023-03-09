#!/bin/bash -eux

export SCORE_DIR=$PWD

mkdir -p /build || true
chown -R $(whoami) /build
cd /build

cmake $SCORE_DIR \
 -GNinja \
 -DQT_VERSION=Qt5 \
 -DCMAKE_UNITY_BUILD=1 \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_TOOLCHAIN_FILE=/opt/ossia-sdk-rpi/toolchain.cmake \
 -DOSSIA_SDK=/opt/ossia-sdk-rpi/pi/sysroot/opt/ossia-sdk-rpi \
 -DCMAKE_CXX_FLAGS="-DCMT_FORCE_NON_CLANG=1 -flax-vector-conversions -Wno-psabi" \
 -DSCORE_DEPLOYMENT_BUILD=1 \
 -DOSSIA_ENABLE_KFR=0 \
 -DOSSIA_ENABLE_FFTW=0 \
 -DCMAKE_INSTALL_PREFIX=install \
 -DSCORE_LINKER_SCRIPT="$SCORE_DIR/cmake/Deployment/Linux/AppImage/version"

cmake --build .
cmake --build . --target install/strip

(
  cd install
  rm -rf include lib share/doc
  mkdir lib
  cp /opt/ossia-sdk-rpi/armv8-rpi3-linux-gnueabihf/armv8-rpi3-linux-gnueabihf/lib/libstdc++.so.6 lib/
  cp $SCORE_DIR/cmake/Deployment/Linux/Raspberry/* .
)

export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")
mv install "ossia score-$TAG"
tar caf "$SCORE_DIR/score.tar.gz" "ossia score-$TAG"
