#!/bin/bash

cd /image

NPROC=$(nproc)

GCC=gcc-6.1.0

mkdir gcc-build
(
  cd gcc-build
  ../$GCC/configure --enable-languages=c,c++,lto --enable-gold --enable-plugin --disable-multilib --disable-nls
  make
)
