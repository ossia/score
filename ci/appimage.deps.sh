#!/bin/bash -eux

source ci/common.setup.sh

# On the self-hosted host job these are pre-installed and we are not root; only
# apt-install when something is missing (the hosted-runner path).
if ! command -v wget >/dev/null 2>&1 || ! dpkg -s libfuse2 >/dev/null 2>&1 || ! dpkg -s desktop-file-utils >/dev/null 2>&1; then
  $SUDO apt-get update -qq
  $SUDO apt-get install -y wget libfuse2 desktop-file-utils
fi

if [[ "${CPU_ARCH}" == "aarch64" ]]; then
  export CPU_ARCH_SUFFIX="-aarch64"
else
  export CPU_ARCH_SUFFIX="-x86_64"
fi

wget -nv "https://github.com/ossia/sdk/releases/download/sdk37/sdk-linux${CPU_ARCH_SUFFIX}.tar.xz"
tar xaf sdk-linux${CPU_ARCH_SUFFIX}.tar.xz
rm -rf  sdk-linux${CPU_ARCH_SUFFIX}.tar.xz

source ci/common.deps.sh LINUX
