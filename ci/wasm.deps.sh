#!/bin/bash -eux

# Note: this is run from an ArchLinux container due to too recent glibc
pacman -Syyu --noconfirm
pacman -S wget ninja --noconfirm

export SDK_ARCHIVE=sdk-wasm.tar.xz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk19/$SDK_ARCHIVE -O $SDK_ARCHIVE

mkdir -p /opt/ossia-sdk-wasm
chown -R $(whoami) /opt/ossia-sdk-wasm
chmod -R a+rwx /opt/ossia-sdk-wasm
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-wasm/

rm *.tar.*