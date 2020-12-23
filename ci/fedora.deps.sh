#!/bin/bash -eux

# Needed for ffmpeg-devel
dnf -y install https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
dnf -y install https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

# TODO fix SDL2-devel SDL2-static -fPIC issue

dnf -y install \
     cmake ninja-build \
     ffmpeg-devel llvm-devel \
     alsa-lib-devel portaudio-devel \
     jack-audio-connection-kit-devel \
     lilv-devel suil-devel \
     fftw-devel \
     avahi-devel \
     bluez-libs-devel \
     qt5-qtbase-devel qt5-qttools-devel qt5-qtserialport-devel qt5-qtwebsockets-devel qt5-qtdeclarative-devel \
     qt5-qtbase-private-devel  qt5-qtdeclarative-private-devel
