#!/bin/bash -eux

set +e

export HOMEBREW_NO_AUTO_UPDATE=1
brew update && (brew list cmake || brew install cmake)
brew install ninja qt boost ffmpeg@7 fftw portaudio jack sdl lv2 lilv suil freetype
brew uninstall --ignore-dependencies qt@5 || true

source ci/common.deps.sh
