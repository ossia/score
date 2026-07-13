#!/bin/bash -eux

# Built on ubuntu-24.04 (see .github/workflows/wasm.yaml) -- the SAME image the
# wasm SDK is built on, so the SDK's prebuilt qt6-host tools find a matching
# system ICU (24.04 ships ICU 74). The runtime libs below are what those host
# tools (qmlimportscanner, moc, ...) link against.
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  wget ca-certificates git tar xz-utils ninja-build cmake python3 \
  libicu74 libdouble-conversion3 libb2-1 libglib2.0-0 \
  libpcre2-16-0 libpcre2-8-0 zlib1g libfontconfig1 libgl1 libegl1

# The wasm SDK built by ossia/sdk CI (Qt 6.12, emsdk 5.0.5, ffmpeg 8.1).
export SDK_ARCHIVE=sdk-wasm.tar.xz
wget -nv https://github.com/ossia/sdk/releases/download/sdk37/$SDK_ARCHIVE -O $SDK_ARCHIVE

sudo mkdir -p /opt/ossia-sdk-wasm
sudo chown -R "$(whoami)" /opt/ossia-sdk-wasm
chmod -R a+rwx /opt/ossia-sdk-wasm
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-wasm/

rm *.tar.*

source ci/common.deps.sh WASM
