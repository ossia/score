#!/usr/bin/env bash

source ci/common.setup.sh

pkg install -y \
  bash \
  boost-libs \
  cmake git \
  llvm18 \
  qt6-base qt6-declarative qt6-shadertools qt6-websockets qt6-serialport qt6-scxml qt6-svg qt6-connectivity \
  ffmpeg \
  lilv suil lv2 \
  libcoap \
  portaudio \
  pd \
  libfmt spdlog \
  rubberband libsamplerate libsndfile \
  libcoap \
  freetype2 harfbuzz fontconfig \
  alsa-lib \
  jackit

source ci/common.deps.sh
