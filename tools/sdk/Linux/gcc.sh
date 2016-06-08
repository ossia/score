#!/bin/bash

cd /image

NPROC=$(nproc)

mkdir gcc-build
(
  cd gcc-build
  ../combined/configure --enable-languages=c,c++,lto --enable-gold --enable-plugin --disable-multilib --disable-nls
  make
)
