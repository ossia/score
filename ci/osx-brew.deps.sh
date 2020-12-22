#!/bin/bash -eux

set +e

export HOMEBREW_NO_AUTO_UPDATE=1
brew install ninja qt5 boost cmake ffmpeg portaudio jack fftw