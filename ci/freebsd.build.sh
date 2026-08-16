#!/usr/bin/env bash
# The options have to be set here, not on the shebang line: env(1) does not
# split its first operand, so "#!/usr/bin/env bash -e" looks for a program
# literally named "bash -e" and exits 127 before running a single line.
# Note that bash is /usr/local/bin/bash on FreeBSD, hence env rather than the
# "#!/bin/bash -e" the other ci/*.build.sh use.
set -e

export SCORE_DIR=$PWD

mkdir -p /build || true
cd /build

cmake $SCORE_DIR \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=install \
  -DSCORE_DYNAMIC_PLUGINS=1 \
  -DSCORE_DISABLED_PLUGINS="score-plugin-pd" \
  -DCMAKE_CXX_FLAGS="-fexperimental-library" \
  -DSCORE_PCH=1

cmake --build .
cmake --build . --target install
