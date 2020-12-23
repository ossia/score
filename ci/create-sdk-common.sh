#!/bin/bash -eux

mkdir -p "$INCLUDE/qt"
rsync -ar "$OSSIA_SDK/qt5-static/include/" "$INCLUDE/qt/"

if [[ -d "$OSSIA_SDK/llvm-libs/include" ]]; then
  rsync -ar "$OSSIA_SDK/llvm-libs/include/" "$INCLUDE/"
else
  rsync -ar "$OSSIA_SDK/llvm/include/" "$INCLUDE/"
fi

rsync -ar "$OSSIA_SDK/ffmpeg/include/" "$INCLUDE/"
rsync -ar "$OSSIA_SDK/fftw/include/" "$INCLUDE/"
rsync -ar "$OSSIA_SDK/portaudio/include/" "$INCLUDE/"

if [[ -d "$OSSIA_SDK/openssl/include" ]]; then
  rsync -ar "$OSSIA_SDK/openssl/include/" "$INCLUDE/"
fi

BOOST=$(find "$SCORE_DIR/3rdparty/libossia/3rdparty/" -maxdepth 2 -name boost -type d | sort | tail -1)
if [[ -d "$BOOST" ]]; then
  cp -rf "$BOOST" "$INCLUDE/"
else
  BOOST=$(find /usr/local/Cellar/boost/ -mindepth 3 -maxdepth 3 -name boost -type d | sort | tail -1)
  if [[ -d "$BOOST" ]]; then
    cp -rf "$BOOST" "$INCLUDE/"
  fi
fi