#!/bin/bash

cd /image

## GCC deps

BINUTILS=binutils-2.28
MPC=mpc-1.0.3
MPFR=mpfr-3.1.5
GMP=gmp-6.1.2
GCC=gcc-6.3.0
ISL=isl-0.16.1
CLOOG=cloog-0.18.4

wget -nv "http://isl.gforge.inria.fr/$ISL.tar.bz2"
wget -nv "https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.bz2"
wget -nv "http://ftp.gnu.org/gnu/gmp/$GMP.tar.bz2"
wget -nv "ftp://ftp.gnu.org/gnu/mpc/$MPC.tar.gz"
wget -nv "http://www.mpfr.org/mpfr-current/$MPFR.tar.bz2"
wget -nv "http://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.bz2"
wget -nv "http://www.bastoul.net/cloog/pages/download/count.php3?url=./$CLOOG.tar.gz" -O $CLOOG.tar.gz

tar xaf "$GMP.tar.bz2"
tar xaf "$MPC.tar.gz"
tar xaf "$MPFR.tar.bz2"
tar xaf "$ISL.tar.bz2"
tar xaf "$CLOOG.tar.gz"
tar xaf "$BINUTILS.tar.bz2"
tar xaf "$GCC.tar.bz2"

mkdir combined
(
  cd combined
  ln -s ../$GCC/* .
  ln -s ../$BINUTILS/* .
  ln -s ../$GMP gmp
  ln -s ../$MPC mpc
  ln -s ../$MPFR mpfr
  ln -s ../$ISL isl
  ln -s ../$CLOOG cloog
)

## GCC build
NPROC=$(nproc)

export PATH=/opt/gcc-6/bin:/opt/gcc-6-temp/bin:$PATH

mkdir -p /opt/gcc-6-temp
mkdir -p gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto \
                        --enable-gold \
                        --enable-plugins \
                        --enable-plugin \
                        --disable-nls \
                        --enable-werror=no \
                        --disable-multilib \
                        --prefix=/opt/gcc-6-temp 

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
  export LD_LIBRARY_PATH=/opt/gcc-6-temp/lib

  ../combined/configure --enable-languages=c,c++,lto \
                        --enable-gold \
                        --enable-plugins \
                        --enable-plugin \
                        --disable-nls \
                        --enable-werror=no \
                        --disable-multilib \
                        --with-build-config=bootstrap-lto \
                        --enable-checking=none \
                        --prefix=/opt/gcc-6 \
                        --with-build-time-tools=/opt/gcc-6-temp/bin

  make BOOT_CFLAGS="-O3 -g0" -j$NPROC && make install-strip
)

rm -rf /opt/gcc-6-temp
cd /
rm -rf /image
