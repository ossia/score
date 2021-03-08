#!/bin/bash

cinst -y ninja

set -x
mkdir /c/ossia-sdk
cd /c/ossia-sdk
curl -L https://github.com/ossia/sdk/releases/download/sdk19/sdk-mingw.7z --output sdk-mingw.7z
7z x sdk-mingw.7z
rm sdk-mingw.7z
ls
