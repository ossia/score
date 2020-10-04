#!/bin/bash -eux
if [[ "$TRAVIS_TAG" = "" ]];
then
  TRAVIS_TAG=devel
fi


export TAG=$(echo $TRAVIS_TAG | tr -d v)
case "$CONF" in
  osx-package)
    cd /Users/travis/build/ossia/score/bundle/
    mkdir /Users/travis/build/ossia/score/deploy

    zip -r -9 "score.zip" score.app
    mv "score.zip" "/Users/travis/build/ossia/score/deploy/ossia score-$TAG-macOS.zip"
    mv "mac-sdk.zip" "/Users/travis/build/ossia/score/deploy/"
  ;;
  linux-package-appimage)
    cd /home/travis/build/ossia/score/
    mkdir deploy
    mv "Score.AppImage" "deploy/ossia score-$TAG-linux-amd64.AppImage"
    mv "linux-sdk.zip" "deploy/"
  ;;
  tarball)
    cd /home/travis/build/ossia/score/
    mkdir deploy
    mv "ossia-score.tar.xz" "deploy/ossia score-$TAG-src.tar.xz"
    mv "ossia-score.tar.xz.asc" "deploy/ossia score-$TAG-src.tar.xz.asc"
  ;;
esac
