#!/bin/sh
sudo docker build --squash --compress --force-rm  -f Dockerfile.llvm -t ossia/score-linux-llvm . 
sudo docker rm llvm-build-vm
sudo docker run --name llvm-build-vm -v "$(pwd)"/Recipe.llvm:/Recipe -w="/" -e TRAVIS_COMMIT=`git rev-parse HEAD` ossia/score-linux-llvm /bin/bash /Recipe
sudo docker cp llvm-build-vm:/score.AppDir.txz .
tar xaf score.AppDir.txz
cp AppRun score.AppDir/
./appimagetool-x86_64.AppImage -n score.AppDir Score.AppImage

