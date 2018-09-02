#!/bin/bash

## Faust
export PATH=/cmake/bin:$PATH
git clone --depth=1 https://github.com/grame-cncm/faust
cd faust/build
echo '
set ( ASMJS_BACKEND  OFF CACHE STRING  "Include ASMJS backend" FORCE )
set ( C_BACKEND      COMPILER STATIC DYNAMIC        CACHE STRING  "Include C backend"         FORCE )
set ( CPP_BACKEND    COMPILER STATIC DYNAMIC        CACHE STRING  "Include CPP backend"       FORCE )
set ( FIR_BACKEND    OFF        CACHE STRING  "Include FIR backend"       FORCE )
set ( INTERP_BACKEND OFF        CACHE STRING  "Include INTERPRETER backend" FORCE )
set ( JAVA_BACKEND   OFF        CACHE STRING  "Include JAVA backend"      FORCE )
set ( JS_BACKEND     OFF        CACHE STRING  "Include JAVASCRIPT backend" FORCE )
set ( LLVM_BACKEND   COMPILER STATIC DYNAMIC        CACHE STRING  "Include LLVM backend"      FORCE )
set ( OLDCPP_BACKEND OFF        CACHE STRING  "Include old CPP backend"   FORCE )
set ( RUST_BACKEND   OFF        CACHE STRING  "Include RUST backend"      FORCE )
set ( WASM_BACKEND   OFF   CACHE STRING  "Include WASM backend"  FORCE )
' > backends/llvm.cmake
mkdir -p faustdir
cd faustdir
cmake -C ../backends/llvm.cmake  .. -DINCLUDE_OSC=0 -DINCLUDE_HTTP=0 -DINCLUDE_EXECUTABLE=0 -DINCLUDE_STATIC=1
BACKENDS=llvm.cmake make configstatic
make -j$(nproc)
make install

## FFMPEG
yum install -y nasm
wget -nv https://ffmpeg.org/releases/ffmpeg-4.0.tar.bz2
tar -xaf ffmpeg-4.0.tar.bz2
cd ffmpeg-4.0
./configure --arch=x86_64 --cpu=x86_64 --disable-doc --disable-ffmpeg --disable-ffplay --disable-debug --prefix=/usr/local --pkg-config-flags="--static" --enable-gpl --enable-version3 --disable-openssl --disable-securetransport --disable-videotoolbox --disable-network --disable-iconv --disable-lzma
make -j$(nproc)
make install
cd ..

## PortAudio
yum -y install alsa-lib-devel
wget -nv http://www.portaudio.com/archives/pa_snapshot.tgz
tar -xaf pa_snapshot.tgz
cd portaudio/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
make install
cp libportaudio_static.a /usr/local/lib
cp libportaudio_static.a /usr/local/lib64
cd ../..
