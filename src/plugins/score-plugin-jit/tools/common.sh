#!/bin/bash

mkdir "$HEADERS/qt"
rsync -ar "$OSSIA_SDK/qt5-static/include/" "$HEADERS/qt/"
rsync -ar "$OSSIA_SDK/llvm/include/" "$HEADERS/"
rsync -ar "$OSSIA_SDK/ffmpeg/include/" "$HEADERS/"
rsync -ar "$OSSIA_SDK/fftw/include/" "$HEADERS/"
rsync -ar "$OSSIA_SDK/portaudio/include/" "$HEADERS/"

