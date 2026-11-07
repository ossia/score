#!/bin/bash -e

# GitHub's macOS runners: 3 cores, 7 GB. See SCORE_HEAVY_COMPILE_JOBS in CMakeLists.txt.
JOB_POOLS=()
if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
  JOB_POOLS=(-DSCORE_COMPILE_JOBS=2 -DSCORE_HEAVY_COMPILE_JOBS=1)
fi

mkdir build
cd build

cmake .. \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DCMAKE_UNITY_BUILD=1 \
  "${JOB_POOLS[@]}"

cmake --build .
cmake --build . --target install
