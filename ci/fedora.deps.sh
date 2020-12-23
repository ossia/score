#!/bin/bash -eux

# Needed for ffmpeg-devel
dnf install -y https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm
dnf install -y https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

dnf install -y \
     cmake ninja-build \
     ffmpeg-devel llvm-devel \
     alsa-lib-devel portaudio-devel \
     jack-audio-connection-kit-devel \
     lilv-devel suil-devel \
     SDL2-devel SDL2-static \
     fftw-devel \
     avahi-devel \
     bluez-libs-devel \
     qt5-qtbase-devel qt5-qttools-devel qt5-qtserialport-devel qt5-qtwebsockets-devel qt5-qtdeclarative-devel \
     qt5-qtbase-private-devel  qt5-qtdeclarative-private-devel
