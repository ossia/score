#!/bin/sh

docker pull iscore/i-score-package-linux
docker run --name buildvm iscore/i-score-package-linux /bin/bash Recipe
docker cp buildvm:/i-score.AppDir.txz .

tar xaf i-score.AppDir.txz

cp ../CMake/Deployment/Linux/AppImage/AppRun i-score.AppDir/
chmod a+rwx i-score.AppDir/AppRun

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

./appimagetool-x86_64.AppImage "$APP.AppDir" "i-score.AppImage"

chmod a+rwx i-score.AppImage
