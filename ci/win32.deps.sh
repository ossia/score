#!/bin/bash

choco install -y ninja
choco install -y rsync
choco install -y nsis

(
set -x
mkdir /c/ossia-sdk-x86_64
cd /c/ossia-sdk-x86_64
curl -L https://github.com/ossia/sdk/releases/download/sdk34/sdk-mingw-x86_64.7z --output sdk-mingw-x86_64.7z
7z x sdk-mingw-x86_64.7z
rm sdk-mingw-x86_64.7z
ls
)

source ci/common.deps.sh
