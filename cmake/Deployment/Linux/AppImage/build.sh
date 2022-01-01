#!/bin/sh
sudo rm -rf *AppDir*
sudo rm -rf *AppImage
sudo rm -rf /tmp/build
mkdir /tmp/build

docker build --squash --compress --force-rm  -f Dockerfile.llvm -t ossia/score-linux-llvm .

export SOURCE=$(git rev-parse --show-toplevel)

docker run --rm -it \
           -v "$(pwd)"/Recipe.llvm:/Recipe \
           -e TOOLCHAIN=appimage-debug \
           --mount type=bind,source=$SOURCE,target=/score \
           --mount type=bind,source="/tmp/build",target=/build \
           --mount type=bind,source="/opt/ossia-sdk",target=/opt/ossia-sdk \
           -w="/" \
           ossia/score-linux-llvm \
           /bin/bash Recipe

wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O appimagetool-x86_64.AppImage 
chmod a+x appimagetool-x86_64.AppImage
wget "https://github.com/probonopd/AppImageKit/releases/download/continuous/AppRun-x86_64" -O AppRun-x86_64
chmod a+x AppRun-x86_64
sudo chown -R $(whoami) /tmp/build
cp AppRun-x86_64 /tmp/build/score.AppDir/AppRun
cp ossia-score.desktop /tmp/build/score.AppDir/
cp $SOURCE/src/lib/resources/ossia-score.png /tmp/build/score.AppDir/
cp $SOURCE/src/lib/resources/ossia-score.png /tmp/build/score.AppDir/.DirIcon
cp -rf /tmp/build/score.AppDir .
./appimagetool-x86_64.AppImage -n score.AppDir Score.AppImage

