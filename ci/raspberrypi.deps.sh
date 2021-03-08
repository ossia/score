#!/bin/bash -eux

# Note: this is run from an ArchLinux container due to too recent glibc
pacman -Syyu --noconfirm
pacman -S wget ninja

export SDK_ARCHIVE=sdk-rpi.tar.xz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk19/$SDK_ARCHIVE -O $SDK_ARCHIVE
mkdir -p /opt/ossia-sdk-rpi
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-rpi/

wget -nv https://raw.githubusercontent.com/ossia/sdk/master/ARM/RPi4/toolchain.cmake -O /opt/ossia-sdk-rpi/toolchain.cmake

rm *.tar.*