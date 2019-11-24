#!/bin/sh
set +eux
export CC=/usr/bin/clang-9
export CXX=/usr/bin/clang++-9
export CMAKE_BIN=$(readlink -f "$(find ../cmake-latest/bin -name cmake -type f )")
export CMAKE_COMMON_FLAGS="-GNinja -DPORTAUDIO_ONLY_DYNAMIC=1 -DOSSIA_USE_FAST_LINKER=1"
QT_ENV_SCRIPT=$(find /opt -name 'qt*-env.sh')
source $QT_ENV_SCRIPT

set -eux
