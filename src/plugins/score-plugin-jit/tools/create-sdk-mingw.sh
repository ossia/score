#!/bin/bash

if [[ "x$SCORE_DIR" == "x" ]]; then
  echo "SCORE_DIR not set"
  exit 1
fi

if [[ "x$SDK_DIR" == "x" ]]; then
  echo "SDK_DIR not set"
  exit 1
fi

export SCORE="$SCORE_DIR"
export SRC="/c/score-sdk"
export DST="$SDK_DIR" 

mkdir -p "$DST/usr/include"
mkdir -p "$DST/usr/lib"

cd "$DST/usr"
cp -rf "$SRC/qt5-static/include/"* "$DST/usr/include/"
cp -rf "$SRC/ffmpeg/include/"* "$DST/usr/include/"
cp -rf "$SRC/fftw/include/"* "$DST/usr/include/"
cp -rf "$SRC/portaudio/include/"* "$DST/usr/include/"
cp -rf "$SRC/openssd/include/"* "$DST/usr/include/"

if [[ -d "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0" ]]; then
  cp -rf "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0/boost" "$DST/usr/include/"
fi

(
# Copy our compiler's intrinsincs
export LLVM_VER=$(ls $SRC/llvm-libs/lib/clang/)
mkdir -p "$DST/usr/lib/clang/$LLVM_VER/include"
cp -rf "$SRC/llvm-libs/lib/clang/$LLVM_VER/include/"* "$DST/usr/lib/clang/$LLVM_VER/include/"
)

(
# Copy the mingw API
cp -rf "$SRC/llvm/include/"* "$DST/usr/include/"
)
