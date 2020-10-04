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
rsync -ar "$SRC/qt5-static/include/" "include/"
rsync -ar "$SRC/ffmpeg/include/" "include/"
rsync -ar "$SRC/fftw/include/" "include/"
rsync -ar "$SRC/portaudio/include/" "include/"
rsync -ar "$SRC/openssl/include/" "include/"

if [[ -d "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0" ]]; then
  rsync -ar "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0/boost" "include/"
fi

(
# Copy our compiler's intrinsincs
export LLVM_VER=$(ls $SRC/llvm-libs/lib/clang/)
mkdir -p "$DST/usr/lib/clang/$LLVM_VER/include"
rsync -ar "$SRC/llvm-libs/lib/clang/$LLVM_VER/include/" "$DST/usr/lib/clang/$LLVM_VER/include/"
)

(
# Copy the mingw API
rsync -ar "$SRC/llvm/include/" "$DST/usr/include/"
)
