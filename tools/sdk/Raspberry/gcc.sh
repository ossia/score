#!/bin/bash -eux

cd /image
NPROC=$(nproc)

export PATH=/opt/gcc-6/bin:/opt/gcc-6-temp/bin:$PATH

mkdir -p /opt/gcc-6-temp
mkdir -p gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-nls --enable-werror=no --prefix=/opt/gcc-6-temp
  make -j$NPROC && make install-strip
)
rm -rf gcc-build

# Now rebuild all the binutils with LTO

mkdir -p /opt/gcc-6
mkdir gcc-build-2
(
  cd gcc-build-2
  export CC=/opt/gcc-6-temp/bin/gcc
  export CXX=/opt/gcc-6-temp/bin/g++

  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-nls --enable-werror=no --with-build-config=bootstrap-lto --enable-checking=none --prefix=/opt/gcc-6 --sysroot=/opt/gcc-6-temp
  make BOOT_CFLAGS="-O3 -g0" -j$NPROC && make install-strip
)

rm -rf /opt/gcc-6-temp
