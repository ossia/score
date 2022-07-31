#!/bin/bash

cinst -y ninja
cinst -y rsync

(
set -x
mkdir /c/ossia-sdk-msvc
cd /c/ossia-sdk-msvc
curl -L https://github.com/ossia/sdk/releases/download/sdk25/sdk-msvc.7z --output sdk-msvc.7z
7z x sdk-msvc.7z
rm sdk-msvc.7z
ls
)

source ci/common.deps.sh
