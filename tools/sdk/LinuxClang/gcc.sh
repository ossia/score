#!/bin/bash

cd /image
NPROC=$(nproc)

mkdir gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-multilib --disable-nls --enable-werror=no --enable-threads
  make BOOT_CFLAGS="-O3 -g0" -j$NPROC && make install-strip && rsync -rtva /usr/local/ /usr/
  # make -j$NPROC && make install-strip && rsync -rtv /usr/local/ /usr/
)

rm -rf gcc-build
# Now rebuild all the binutils with LTO
#~ mkdir gcc-build-2
#~ (
  #~ cd gcc-build-2
  #~ export CC=/usr/local/bin/gcc
  #~ export CXX=/usr/local/bin/g++
  #~ export PATH=/usr/local/bin:$PATH

  #~ ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-multilib --disable-nls --enable-werror=no --with-build-config=bootstrap-lto --enable-checking=none --enable-threads
  #~ make BOOT_CFLAGS="-O3 -g0" -j$NPROC && make install-strip && rsync -rtva /usr/local/ /usr/
#~ )
#~ rm -rf gcc-build-2

# Alternative, optimized :


# ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugins --enable-plugin --disable-multilib --disable-nls --enable-werror=no --prefix=/usr --with-build-config=bootstrap-lto --enable-checking=none
# make BOOT_CFLAGS="-O3 -g0" -j8
# make install-strip
