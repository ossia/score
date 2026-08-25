#!/bin/bash -eux

source ci/common.setup.sh

# On the self-hosted host job these are pre-installed and we are not root; only
# apt-install when something is missing (the hosted-runner path).
# Check the tools themselves, not package names (libfuse2 is libfuse2t64 on
# 24.04+). With APPIMAGE_EXTRACT_AND_RUN=1 the build needs no FUSE, so libfuse2
# is not required here. On the self-hosted host job these are pre-installed and
# we are not root, so this apt block is skipped.
if ! command -v wget >/dev/null 2>&1 || ! command -v desktop-file-validate >/dev/null 2>&1; then
  $SUDO apt-get update -qq
  $SUDO apt-get install -y wget desktop-file-utils libfuse2t64 || $SUDO apt-get install -y wget desktop-file-utils libfuse2
fi

if [[ "${CPU_ARCH}" == "aarch64" ]]; then
  export CPU_ARCH_SUFFIX="-aarch64"
else
  export CPU_ARCH_SUFFIX="-x86_64"
fi

wget -nv "https://github.com/ossia/sdk/releases/download/sdk39/sdk-linux${CPU_ARCH_SUFFIX}.tar.xz"
tar xaf sdk-linux${CPU_ARCH_SUFFIX}.tar.xz
rm -rf  sdk-linux${CPU_ARCH_SUFFIX}.tar.xz

source ci/common.deps.sh LINUX
