#!/bin/bash

choco install -y ninja
choco install -y rsync

(
set -x
mkdir /c/ossia-sdk
cd /c/ossia-sdk
curl -L https://github.com/ossia/sdk/releases/download/sdk29/sdk-mingw.7z --output sdk-mingw.7z
7z x sdk-mingw.7z
rm sdk-mingw.7z
ls
)

source ci/common.deps.sh
