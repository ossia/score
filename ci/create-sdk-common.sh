#!/bin/bash -eux

mkdir -p "$INCLUDE/qt"

cp -rf "$OSSIA_SDK/qt6-static/include/." "$INCLUDE/qt/"

if [[ -d "$OSSIA_SDK/llvm-libs/include" ]]; then
  cp -rf "$OSSIA_SDK/llvm-libs/include/." "$INCLUDE/"
else
  cp -rf "$OSSIA_SDK/llvm/include/." "$INCLUDE/"
fi

cp -rf "$OSSIA_SDK/ffmpeg/include/." "$INCLUDE/" || true
cp -rf "$OSSIA_SDK/fftw/include/." "$INCLUDE/" || true
cp -rf "$OSSIA_SDK/portaudio/include/." "$INCLUDE/" || true

if [[ -d "$OSSIA_SDK/openssl/include" ]]; then
  cp -rf "$OSSIA_SDK/openssl/include/." "$INCLUDE/"
fi

cp -rf "$SCORE_DIR/3rdparty/avendish/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/csv2/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/DSPFilters/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xsimd/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xtensor/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/xtl/include/." "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/eigen/Eigen" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/Gamma/Gamma" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/vcglib/vcg" "$INCLUDE/"
cp -rf "$SCORE_DIR/3rdparty/vcglib/wrap" "$INCLUDE/"


BOOST=$(find "$SCORE_DIR/3rdparty/libossia/3rdparty/" -maxdepth 2 -name boost -type d | sort | tail -1)
if [[ -d "$BOOST" ]]; then
  cp -rf "$BOOST" "$INCLUDE/"
else
  BOOST=$(find /usr/local/Cellar/boost/ -mindepth 3 -maxdepth 3 -name boost -type d | sort | tail -1)
  if [[ -d "$BOOST" ]]; then
    cp -rf "$BOOST" "$INCLUDE/"
  fi
fi