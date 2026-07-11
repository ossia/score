#!/bin/bash

choco install -y ninja
choco install -y rsync
choco install -y nsis

# Pick the SDK matching the runner architecture (x64 -> x86_64, arm64 -> aarch64).
if [[ "${RUNNER_ARCH:-X64}" == "ARM64" ]]; then
  SDK_ARCH=aarch64
else
  SDK_ARCH=x86_64
fi

(
set -x
mkdir /c/ossia-sdk-$SDK_ARCH
cd /c/ossia-sdk-$SDK_ARCH
curl -L https://github.com/ossia/sdk/releases/download/sdk37/sdk-mingw-$SDK_ARCH.7z --output sdk-mingw-$SDK_ARCH.7z
7z x sdk-mingw-$SDK_ARCH.7z
rm sdk-mingw-$SDK_ARCH.7z
ls
)

source ci/common.deps.sh WIN32
