#!/bin/bash -eux

cd /image
NPROC=$(nproc)

mkdir gcc-build
(
  cd gcc-build
  # export CFLAGS="-O3 -march=armv7-a -g0"
  # export CXXFLAGS="-O3 -march=armv7-a -g0"
  ../combined/configure \
        --enable-languages=c,c++,lto \
        --enable-gold \
        --enable-plugins \
        --enable-plugin \
        --disable-multilib \
        --disable-nls \
        --enable-werror=no \
        --enable-threads \
        --enable-lto \
        --with-float=hard \
        --with-arch=armv7-a \
        --with-fpu=neon \
        --build=arm-linux-gnueabihf \
        --host=arm-linux-gnueabihf \
        --with-build-config=bootstrap-lto \
        --enable-stage1-checking=release \
        --target=arm-linux-gnueabihf 
  # make BOOT_CFLAGS="-O3 -march=armv7-a -g0" -j$NPROC
  make profiledbootstrap -j$NPROC 
  make install-strip
  rsync -rtva /usr/local/ /usr/
)

rm -rf gcc-build