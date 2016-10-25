#!/bin/sh
set +eux
export CC=gcc-6
export CXX=g++-6
export BOOST_ROOT=/opt/boost
export CMAKE_COMMON_FLAGS="-DOSSIA_DISABLE_COTIRE=1"
QT_ENV_SCRIPT=$(find /opt -name 'qt*-env.sh')
source $QT_ENV_SCRIPT

set -eux
