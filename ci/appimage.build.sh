#!/bin/bash
export BUILD_FOLDER=/tmp/build
export SOURCE_FOLDER=$PWD
wget https://github.com/ossia/sdk/releases/download/sdk19/sdk-linux.tar.xz
tar xaf sdk-linux.tar.xz
rm -rf  sdk-linux.tar.xz

mkdir -p $BUILD_FOLDER
ln -s $BUILD_FOLDER build
docker pull ossia/score-package-linux
docker run \
           -v "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/Recipe.llvm:/Recipe" \
           --mount type=bind,source="$PWD/opt/ossia-sdk",target=/opt/ossia-sdk \
           --mount type=bind,source="$SOURCE_FOLDER",target=/score \
           --mount type=bind,source="$BUILD_FOLDER",target=/build \
           ossia/score-package-linux \
           /bin/bash /Recipe

sudo chown -R $(whoami) $BUILD_FOLDER

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/AppRun-x86_64"
chmod a+x AppRun-x86_64
cp AppRun-x86_64 build/score.AppDir/AppRun

./appimagetool-x86_64.AppImage -n "build/score.AppDir" "Score.AppImage"

chmod a+rwx Score.AppImage
(
    cd $BUILD_FOLDER/SDK
    zip -r -q -9 $SOURCE_FOLDER/linux-sdk.zip usr
)
