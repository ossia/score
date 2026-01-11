#!/bin/bash -eux

sudo apt-get update -qq
sudo apt-get install wget libfuse2 desktop-file-utils

if [[ "${CPU_ARCH}" == "aarch64" ]]; then
  export CPU_ARCH_SUFFIX="-aarch64"
else
  export CPU_ARCH_SUFFIX="-x86_64"
fi

wget -nv "https://github.com/ossia/sdk/releases/download/sdk35/sdk-linux${CPU_ARCH_SUFFIX}.tar.xz"
tar xaf sdk-linux${CPU_ARCH_SUFFIX}.tar.xz
rm -rf  sdk-linux${CPU_ARCH_SUFFIX}.tar.xz

source ci/common.deps.sh
