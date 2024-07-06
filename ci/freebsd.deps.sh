#!/bin/bash -eux

source ci/common.setup.sh

pkg install -y
  boost-libs \
  cmake git \
  clang \
  qt6-base qt6-declarative qt6-shadertools qt6-websockets qt6-serialport qt6-scxml \
  ffmpeg \
  lilv suil lv2 \
  libcoap \
  portaudio \
  pd \
  libfmt spdlog \
  rubberband libsamplerate libsndfile \
  libcoap \
  jack \
  freetype2 harfbuzz fontconfig

source ci/common.deps.sh
