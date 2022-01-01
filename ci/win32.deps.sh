#!/bin/bash

cinst -y ninja
cinst -y rsync

set -x
mkdir /c/ossia-sdk
cd /c/ossia-sdk
curl -L https://github.com/ossia/sdk/releases/download/sdk23/sdk-mingw.7z --output sdk-mingw.7z
7z x sdk-mingw.7z
rm sdk-mingw.7z
ls
