#!/bin/bash

mkdir build
cd build

cmake .. \
  -GNinja \
  -DQT_VERSION="Qt6;6.2" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DCMAKE_UNITY_BUILD=1

cmake --build .
cmake --build . --target install
