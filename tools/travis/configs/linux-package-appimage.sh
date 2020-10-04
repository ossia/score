#!/bin/sh

mkdir -p build

docker pull ossia/score-package-linux
docker run \
           -v "$(pwd)"/cmake/Deployment/Linux/AppImage/Recipe.llvm:/Recipe \
           --mount type=bind,source="$(pwd)",target=/score \
           --mount type=bind,source="$(pwd)/build",target=/build \
           ossia/score-package-linux \
           /bin/bash /Recipe

sudo chown -R $(whoami) build

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/AppRun-x86_64"
chmod a+x AppRun-x86_64
cp AppRun-x86_64 build/score.AppDir/AppRun

./appimagetool-x86_64.AppImage -n "build/score.AppDir" "Score.AppImage"

chmod a+rwx Score.AppImage
(cd build/SDK; zip ../../linux-sdk.zip -r usr -9)
