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
export SRC="/opt/score-sdk-osx"
export DST="$SDK_DIR"

mkdir -p "$DST/usr/include"
mkdir -p "$DST/usr/lib"

cd "$DST/usr"
rsync -ar "$SRC/qt5-static/include/" "include/"
rsync -ar "$SRC/ffmpeg/include/" "include/"
rsync -ar "$SRC/fftw/include/" "include/"
rsync -ar "$SRC/portaudio/include/" "include/"
rsync -ar "$SRC/openssl/include/" "include/"
rsync -ar "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/" "include/"
rsync -ar "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include/c++" "include/"

if [[ -d "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0" ]]; then
  rsync -ar "$SCORE/3rdparty/libossia/3rdparty/boost_1_73_0/boost" "include/"
else 
  cp -rf "/usr/local/Cellar/boost/1.73.0/include/boost" "include/"
fi

export CLANG_VER=$(ls /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/)
mkdir -p "$DST/usr/lib/clang/$CLANG_VER"
rsync -ar "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/12.0.0/include" "$DST/usr/lib/clang/$CLANG_VER/"

