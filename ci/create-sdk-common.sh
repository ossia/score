#!/bin/bash -eux

mkdir -p "$INCLUDE/qt"

cp -rf "$OSSIA_SDK/qt5-static/include/." "$INCLUDE/qt/"

if [[ -d "$OSSIA_SDK/llvm-libs/include" ]]; then
  cp -rf "$OSSIA_SDK/llvm-libs/include/." "$INCLUDE/"
else
  cp -rf "$OSSIA_SDK/llvm/include/." "$INCLUDE/"
fi

cp -rf "$OSSIA_SDK/ffmpeg/include/." "$INCLUDE/"
cp -rf "$OSSIA_SDK/fftw/include/." "$INCLUDE/"
cp -rf "$OSSIA_SDK/portaudio/include/." "$INCLUDE/"

if [[ -d "$OSSIA_SDK/openssl/include" ]]; then
  cp -rf "$OSSIA_SDK/openssl/include/." "$INCLUDE/"
fi

cp -rf "$SCORE_DIR/3rdparty/avendish/include/." "$INCLUDE/"

BOOST=$(find "$SCORE_DIR/3rdparty/libossia/3rdparty/" -maxdepth 2 -name boost -type d | sort | tail -1)
if [[ -d "$BOOST" ]]; then
  cp -rf "$BOOST" "$INCLUDE/"
else
  BOOST=$(find /usr/local/Cellar/boost/ -mindepth 3 -maxdepth 3 -name boost -type d | sort | tail -1)
  if [[ -d "$BOOST" ]]; then
    cp -rf "$BOOST" "$INCLUDE/"
  fi
fi