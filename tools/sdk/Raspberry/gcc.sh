#!/bin/bash -eux

cd /image
NPROC=$(nproc)

export PATH=/opt/gcc-7/bin:/opt/gcc-7-temp/bin:$PATH

export LIBRARY_PATH=/usr/lib/arm-linux-gnueabihf
export C_INCLUDE_PATH=/usr/include/arm-linux-gnueabihf
export CPLUS_INCLUDE_PATH=/usr/include/arm-linux-gnueabihf

mkdir -p /opt/gcc-7-temp
mkdir -p gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-nls --enable-werror=no --with-float=hard --with-arch=armv7-a --with-fpu=vfp --prefix=/opt/gcc-7-temp --build=arm-linux-gnueabihf --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf 
  make -j$NPROC && make install-strip
)
rm -rf gcc-build

# Now rebuild all the binutils with LTO

mkdir -p /opt/gcc-7
mkdir gcc-build-2
(
  cd gcc-build-2
  export CC=/opt/gcc-7-temp/bin/gcc
  export CXX=/opt/gcc-7-temp/bin/g++
  export LD_LIBRARY_PATH=/opt/gcc-7-temp/lib

  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-nls --enable-werror=no --with-build-config=bootstrap-lto --enable-checking=none --with-float=hard  --with-arch=armv7-a --with-fpu=vfp --prefix=/opt/gcc-7 --with-build-time-tools=/opt/gcc-7-temp/bin --build=arm-linux-gnueabihf --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf 
  make BOOT_CFLAGS="-O3 -g0" -j$NPROC && make install-strip
)

rm -rf /opt/gcc-7-temp
