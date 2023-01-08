#!/bin/bash -eux

# Note: this is run from an ArchLinux container due to too recent glibc
pacman -Syyu --noconfirm
pacman -S wget ninja cmake which python tar git double-conversion libb2 icu glib2 pcre2 --noconfirm

export SDK_ARCHIVE=sdk-wasm.tar.xz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk27/$SDK_ARCHIVE -O $SDK_ARCHIVE

wget -nv https://github.com/ossia/score-sdk/releases/download/sdk25/icu71.tar.gz -O icu71.tar.gz
tar xaf icu71.tar.gz
mv libicu* /usr/lib/

mkdir -p /opt/ossia-sdk-wasm
chown -R $(whoami) /opt/ossia-sdk-wasm
chmod -R a+rwx /opt/ossia-sdk-wasm
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-wasm/

rm *.tar.*

source ci/common.deps.sh
