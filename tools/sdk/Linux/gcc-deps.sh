#!/bin/bash

cd /image
BINUTILS=binutils-2.29.1
MPC=mpc-1.0.3
MPFR=mpfr-3.1.6
GMP=gmp-6.1.2
GCC=gcc-7.2.0
ISL=isl-0.18
CLOOG=cloog-0.18.4

wget -nv "http://isl.gforge.inria.fr/$ISL.tar.bz2"
wget -nv "https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.xz"
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

exit 0
