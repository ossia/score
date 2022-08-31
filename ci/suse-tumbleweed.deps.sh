#!/bin/bash -eux

source ci/common.setup.sh

# Disabling libSDL2-devel until -fPIC problem is sorted...

$SUDO zypper -n install \
   cmake ninja gcc11 gcc11-c++ \
   llvm-devel \
   libjack-devel alsa-devel \
   portaudio-devel \
   lv2-devel liblilv-0-devel suil-devel \
   libavahi-devel \
   fftw3-devel fftw3-threads-devel \
   bluez-devel \
   libqt5-qtbase-devel \
   libqt5-qtdeclarative-devel libqt5-qtwebsockets-devel libqt5-qttools libqt5-qtserialport-devel \
   libqt5-qtbase-private-headers-devel libqt5-qtdeclarative-private-headers-devel \
   ffmpeg-4-libavcodec-devel ffmpeg-4-libavdevice-devel ffmpeg-4-libavfilter-devel ffmpeg-4-libavformat-devel ffmpeg-4-libswresample-devel \
   -busybox-which


source ci/common.deps.sh
