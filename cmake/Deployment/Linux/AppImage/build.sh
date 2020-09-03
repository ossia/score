#!/bin/sh
export SOURCE_DIR=$(git rev-parse --show-toplevel)

rm -rf *AppDir*
rm -rf *AppImage
rm -rf build
mkdir build

#docker build --squash --compress --force-rm  -f Dockerfile.llvm -t ossia/score-linux-llvm . 
docker rm -f llvm-build-vm

docker run --name llvm-build-vm -it \
           -v "$(pwd)"/Recipe.llvm:/Recipe \
           --mount type=bind,source="$SOURCE_DIR",target=/score \
           --mount type=bind,source="$(pwd)/build",target=/build \
           -w="/" \
           ossia/score-linux-llvm \
           /bin/bash Recipe

docker cp llvm-build-vm:/score.AppDir.txz .
tar xaf score.AppDir.txz
cp AppRun score.AppDir/
./appimagetool-x86_64.AppImage -n score.AppDir Score.AppImage

