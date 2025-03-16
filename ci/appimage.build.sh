#!/bin/bash
export BUILD_FOLDER=/tmp/build
export SOURCE_FOLDER="$PWD"
wget -nv https://github.com/ossia/sdk/releases/download/sdk32/sdk-linux.tar.xz
tar xaf sdk-linux.tar.xz
rm -rf  sdk-linux.tar.xz

mkdir -p $BUILD_FOLDER
ln -s $BUILD_FOLDER build
docker pull ossia/score-package-linux
docker run \
           -v "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/Recipe.llvm:/Recipe" \
           -e TOOLCHAIN=appimage \
           -e TAG="$GITTAGNOV" \
           --mount type=bind,source="$PWD/opt/ossia-sdk",target=/opt/ossia-sdk \
           --mount type=bind,source="$SOURCE_FOLDER",target=/score \
           --mount type=bind,source="$BUILD_FOLDER",target=/build \
           ossia/score-package-linux \
           /bin/bash /Recipe

sudo chown -R $(whoami) $BUILD_FOLDER


wget -nv "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
chmod a+x appimagetool-x86_64.AppImage

wget -nv "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64"
chmod a+x runtime-x86_64

wget -nv "https://github.com/probonopd/AppImageKit/releases/download/continuous/AppRun-x86_64"
chmod a+x AppRun-x86_64
cp AppRun-x86_64 build/score.AppDir/AppRun
cp "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/ossia-score.desktop" build/score.AppDir/
cp "$SOURCE_FOLDER/src/lib/resources/ossia-score.png" build/score.AppDir/
cp "$SOURCE_FOLDER/src/lib/resources/ossia-score.png" build/score.AppDir/.DirIcon

if [[ ! -f "build/score.AppDir/usr/bin/ossia-score" ]]; then
  echo "Build failure, ossia-score main binary missing ! "
  exit 1
fi

./appimagetool-x86_64.AppImage -n "build/score.AppDir" "Score.AppImage" --runtime-file runtime-x86_64

if [[ ! -f "Score.AppImage" ]]; then
  echo "Build failure, Score.AppImage could not be created"
  exit 1
fi

chmod a+rwx Score.AppImage
(
    cd $BUILD_FOLDER/SDK
    zip -r -q -9 $SOURCE_FOLDER/linux-sdk.zip usr
)
