#!/bin/sh
set +eux
export CC=gcc-5
export CXX=g++-5
export BOOST_ROOT=/opt/boost
QT_ENV_SCRIPT=$(find /opt -name 'qt*-env.sh')
source $QT_ENV_SCRIPT

set -eux
