#!/bin/bash

cinst -y ninja

echo " ============ "
echo "$BUILD_ARTIFACTSTAGINGDIRECTORY"
echo "$BUILD_SOURCESDIRECTORY"
set -x
mkdir /c/score-sdk
cd /c/score-sdk
curl -L https://github.com/ossia/sdk/releases/download/sdk17/score-sdk-mingw.7z --output score-sdk-mingw.7z
7z x score-sdk-mingw.7z
rm score-sdk-mingw.7z
ls