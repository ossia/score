#!/bin/bash -eux

cd /image

NPROC=$(nproc)
BINUTILS=binutils-2.26
MPC=mpc-1.0.3
MPFR=mpfr-3.1.4
GMP=gmp-6.1.0
GCC=gcc-6.1.0
ISL=isl-0.16.1
CLOOG=cloog-0.18.4

wget "http://isl.gforge.inria.fr/$ISL.tar.bz2"
wget "https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.bz2"
wget "http://ftp.gnu.org/gnu/gmp/$GMP.tar.bz2"
wget "ftp://ftp.gnu.org/gnu/mpc/$MPC.tar.gz"
wget "http://www.mpfr.org/mpfr-current/$MPFR.tar.bz2"
wget "http://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.bz2"
wget "http://www.bastoul.net/cloog/pages/download/count.php3?url=./$CLOOG.tar.gz"
tar xaf "$GMP.tar.bz2"
tar xaf "$MPC.tar.gz"
tar xaf "$MPFR.tar.bz2"
tar xaf "$ISL.tar.bz2"
tar xaf "$CLOOG.tar.gz"
tar xaf "$BINUTILS.tar.bz2"
tar xaf "$GCC.tar.bz2"

(
  cd $GCC
  ln -s ../$GMP gmp
  ln -s ../$MPC mpc
  ln -s ../$MPFR mpfr
  ln -s ../$ISL isl
  ln -s ../$CLOOG cloog
  for file in ../$BINUTILS/* ; do ln -s "${file}" ; done
)

mkdir gcc-build
(
  cd gcc-build
  ../$GCC/configure --enable-languages=c,c++,lto --enable-gold --enable-plugin --disable-multilib --disable-nls
  make -j$NPROC
  sudo make -j$NPROC install
)
