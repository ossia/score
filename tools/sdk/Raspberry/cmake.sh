#!/bin/bash -eux
apt-get -y install cmake

cd /image
wget https://cmake.org/files/v3.12/cmake-3.12.1.tar.gz
tar xaf cmake-3.12.1.tar.gz
mkdir cmake-build
cd cmake-build
export CC=/usr/local/bin/gcc
export CXX=/usr/local/bin/g++
export LD_LIBRARY_PATH=/usr/local/lib
export PATH=/usr/local/bin:$PATH
export CFLAGS="-O3 -flto -march=armv7-a -g0"
export CXXFLAGS="-O3 -flto -march=armv7-a -g0"
cmake ../cmake-3.12.1 -DCMAKE_INSTALL_PREFIX=/opt/cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
make install