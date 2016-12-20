#!/bin/bash -eux
apt-get -y install cmake

cd /image
wget https://cmake.org/files/v3.7/cmake-3.7.1.tar.gz
tar xaf cmake-3.7.1.tar.gz
mkdir cmake-build
cd cmake-build
export CC=/opt/gcc-6/bin/gcc
export CXX=/opt/gcc-6/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-6/lib
export PATH=/opt/gcc-6/bin:$PATH
cmake ../cmake-3.7.1 -DCMAKE_INSTALL_PREFIX=/opt/cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
make -j$(nproc)
make install

wget https://sourceforge.net/projects/boost/files/boost/1.62.0/boost_1_62_0.tar.bz2
tar xaf boost_1_62_0.tar.bz2
mv boost_1_62_0 /opt/
