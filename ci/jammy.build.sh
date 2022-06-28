#!/bin/bash

mkdir build
(
cd build

cmake .. \
  -GNinja \
  -DSCORE_DEPLOYMENT_BUILD=1 \
  -DBUILD_SHARED_LIBS=0 \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_UNITY_BUILD=1

cmake --build .
cmake --build . --target install
cmake --build . --target package

mv *.deb ..
)