#!/bin/bash -eux

# Note: this is run from an ArchLinux container due to too recent glibc
pacman -Syyu --noconfirm
pacman -S wget ninja cmake tar sed --noconfirm

export SDK_ARCHIVE=sdk-rpi.tar.xz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk27/$SDK_ARCHIVE -O $SDK_ARCHIVE
mkdir -p /opt/ossia-sdk-rpi
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-rpi/

rm *.tar.*

source ci/common.deps.sh
