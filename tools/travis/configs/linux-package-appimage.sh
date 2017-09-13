#!/bin/sh

docker pull ossia/score-package-linux
docker run --name buildvm -v "$(pwd)"/../CMake/Deployment/Linux/AppImage/Recipe:/Recipe ossia/score-package-linux /bin/bash /Recipe
#docker run --name buildvm ossia/score-package-linux /bin/bash Recipe
docker cp buildvm:/score.AppDir.txz .

tar xaf score.AppDir.txz

cp ../CMake/Deployment/Linux/AppImage/AppRun score.AppDir/
chmod a+rwx score.AppDir/AppRun

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

./appimagetool-x86_64.AppImage -n "score.AppDir" "Score.AppImage"

chmod a+rwx Score.AppImage
