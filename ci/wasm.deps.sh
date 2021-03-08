#!/bin/bash -eux

sudo apt-get update -qq
sudo apt-get install wget ninja-build

export SDK_ARCHIVE=sdk-wasm.tar.xz
wget -nv https://github.com/ossia/score-sdk/releases/download/sdk18/$SDK_ARCHIVE -O $SDK_ARCHIVE

sudo mkdir -p /opt/ossia-sdk-wasm
sudo chown -R $(whoami) /opt/ossia-sdk-wasm
sudo chmod -R a+rwx /opt/ossia-sdk-wasm
tar xaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-wasm/