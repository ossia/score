#!/bin/sh
set +eux
export CC=/usr/bin/clang-6.0
export CXX=/usr/bin/clang++-6.0
export BOOST_ROOT=/opt/boost
export CMAKE_COMMON_FLAGS="-GNinja -DPORTAUDIO_ONLY_DYNAMIC=1"
QT_ENV_SCRIPT=$(find /opt -name 'qt*-env.sh')
source $QT_ENV_SCRIPT

set -eux
