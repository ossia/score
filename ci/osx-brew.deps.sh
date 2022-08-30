#!/bin/bash -eux

set +e

export HOMEBREW_NO_AUTO_UPDATE=1
brew install ninja qt boost cmake ffmpeg fftw portaudio jack sdl lv2 lilv suil freetype
brew uninstall --ignore-dependencies qt5

source ci/common.deps.sh
