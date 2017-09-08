#!/bin/sh

docker pull score/score-package-linux
docker run --name buildvm score/score-package-linux /bin/bash Recipe
docker cp buildvm:/score.AppDir.txz .

tar xaf score.AppDir.txz

cp ../CMake/Deployment/Linux/AppImage/AppRun score.AppDir/
chmod a+rwx score.AppDir/AppRun

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

./appimagetool-x86_64.AppImage "score.AppDir" "score.AppImage"

chmod a+rwx score.AppImage
