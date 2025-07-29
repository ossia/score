#!/bin/bash

choco install -y ninja
choco install -y rsync

(
set -x
mkdir /c/ossia-sdk
cd /c/ossia-sdk
curl -L https://github.com/ossia/sdk/releases/download/sdk33/sdk-mingw-x86_64.7z --output sdk-mingw-x86_64.7z
7z x sdk-mingw-x86_64.7z
rm sdk-mingw-x86_64.7z
ls
)

source ci/common.deps.sh
