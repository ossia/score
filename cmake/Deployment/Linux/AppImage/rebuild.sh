#!/bin/sh

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

sudo chown -R $(whoami) /tmp/build
cp AppRun-x86_64 /tmp/build/score.AppDir/AppRun
cp ossia-score.desktop /tmp/build/score.AppDir/
cp $SOURCE/src/lib/resources/ossia-score.png /tmp/build/score.AppDir/
cp $SOURCE/src/lib/resources/ossia-score.png /tmp/build/score.AppDir/.DirIcon
cp -rf /tmp/build/score.AppDir .
./appimagetool-x86_64.AppImage -n score.AppDir Score.AppImage

