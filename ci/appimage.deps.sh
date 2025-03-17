#!/bin/bash -eux

sudo apt-get update -qq
sudo apt-get install wget libfuse2 desktop-file-utils

if [[ "${CPU_ARCH}" == "aarch64" ]]; then
  export CPU_ARCH_SUFFIX="-aarch64"
else
  export CPU_ARCH_SUFFIX=""
fi

wget -nv "https://github.com/ossia/sdk/releases/download/sdk32/sdk-linux${CPU_ARCH_SUFFIX}.tar.xz"
tar xaf *.tar.xz
rm -rf  *.tar.xz

source ci/common.deps.sh
