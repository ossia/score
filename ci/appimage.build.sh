#!/bin/bash
export BUILD_FOLDER=/tmp/build
export SOURCE_FOLDER="$PWD"
export OSSIA_SDK="/opt/ossia-sdk-${CPU_ARCH}"


mkdir -p $BUILD_FOLDER
ln -s $BUILD_FOLDER build
(
  cd cmake/Deployment/Linux/AppImage/
  docker build \
  --build-arg CPU_ARCH="${CPU_ARCH}" \
  -f Dockerfile.llvm \
  -t ossia/score-package-linux \
  .
)

docker run \
-v "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/Recipe.llvm:/Recipe" \
-e TOOLCHAIN=appimage \
-e TAG="$GITTAGNOV" \
-e OSSIA_SDK="$OSSIA_SDK" \
-e CPU_ARCH="$CPU_ARCH" \
--mount type=bind,source="$PWD/$OSSIA_SDK",target=$OSSIA_SDK \
--mount type=bind,source="$SOURCE_FOLDER",target=/score \
--mount type=bind,source="$BUILD_FOLDER",target=/build \
ossia/score-package-linux \
/bin/bash /Recipe

if [[ ! -f "$BUILD_FOLDER/score.AppDir/usr/bin/ossia-score" ]]; then
  echo "Build failure, ossia-score main binary missing ! "
  exit 1
fi

sudo chown -R $(whoami) "$BUILD_FOLDER"


wget -nv "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${CPU_ARCH}.AppImage"
chmod a+x appimagetool-${CPU_ARCH}.AppImage

wget -nv "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${CPU_ARCH}"
chmod a+x runtime-${CPU_ARCH}

cp "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/AppRun" build/score.AppDir/
cp "$SOURCE_FOLDER/cmake/Deployment/Linux/AppImage/ossia-score.desktop" build/score.AppDir/
sed -i "s/x86_64/$CPU_ARCH/" build/score.AppDir/ossia-score.Desktop
sed -i "s/3.0.0/$GITTAGNOV/" build/score.AppDir/ossia-score.Desktop
cp "$SOURCE_FOLDER/src/lib/resources/ossia-score.png" build/score.AppDir/
cp "$SOURCE_FOLDER/src/lib/resources/ossia-score.png" build/score.AppDir/.DirIcon

./appimagetool-${CPU_ARCH}.AppImage -n "/tmp/build/score.AppDir" "Score.AppImage" --runtime-file runtime-${CPU_ARCH}

if [[ ! -f "Score.AppImage" ]]; then
  echo "Build failure, Score.AppImage could not be created"
  exit 1
fi

chmod a+rwx Score.AppImage
(
  cd $BUILD_FOLDER/SDK
  zip -r -q -9 $SOURCE_FOLDER/linux-sdk.zip usr
)
