#!/bin/bash -eux

export BOOST_VER=63
export BOOST_ADDR=boost_1_63_0

export CC=/opt/gcc-6/bin/gcc
export CXX=/opt/gcc-6/bin/g++
export LD_LIBRARY_PATH=/opt/gcc-6/lib
export PATH=/opt/gcc-6/bin:$PATH

apt-get -y install cmake

mkdir /image
(
    cd /image
    wget https://cmake.org/files/v3.7/cmake-3.7.1.tar.gz
    tar xaf cmake-3.7.1.tar.gz
    mkdir cmake-build
    (
        cd cmake-build
        cmake ../cmake-3.7.1 -DCMAKE_INSTALL_PREFIX=/opt/cmake -DCMAKE_C_COMPILER=$CC -DCMAKE_CXX_COMPILER=$CXX
        make -j$(nproc)
        make install
    )
    
    wget "https://sourceforge.net/projects/boost/files/boost/1.$BOOST_VER.0/$BOOST_ADDR.tar.bz2"
    tar xaf "$BOOST_ADDR.tar.bz2"
    mv "$BOOST_ADDR" /opt/
)

rm -rf /image
