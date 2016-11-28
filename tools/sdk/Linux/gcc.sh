#!/bin/bash

cd /image
NPROC=$(nproc)

mkdir gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-multilib --disable-nls --enable-werror=no --with-build-config=bootstrap-lto --enable-checking=none
  make BOOT_CFLAGS="-O3 -g0" -j$NPROC
  make install-strip
  cp -nrf /usr/local/* /usr/
)

# Alternative, optimized : 


# ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-multilib --disable-nls --enable-werror=no --prefix=/usr --with-build-config=bootstrap-lto --enable-checking=none
# make BOOT_CFLAGS="-O3 -g0" -j8
# make install-strip
