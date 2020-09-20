#!/bin/bash -eux
if [[ "$TRAVIS_TAG" = "" ]];
then
    exit 0
fi


export TAG=$(echo $TRAVIS_TAG | tr -d v)
case "$CONF" in
  osx-package)
    cd /Users/travis/build/ossia/score/bundle/
    mkdir /Users/travis/build/ossia/score/deploy

    zip -r -9 "score.zip" score.app
    mv "score.zip" /Users/travis/build/ossia/score/deploy/"ossia score-$TAG-macOS.zip"
  ;;
  linux-package-appimage)
    cd /home/travis/build/ossia/score/
    mkdir /home/travis/build/ossia/score/deploy
    mv "Score.AppImage" "deploy/ossia score-$TAG-linux-amd64.AppImage"
  ;;
  tarball)
    cd /home/travis/build/ossia/score/
    mkdir /home/travis/build/ossia/score/deploy
    mv "ossia-score.tar.xz" "deploy/ossia score-$TAG-src.tar.xz"
    mv "ossia-score.tar.xz.asc" "deploy/ossia score-$TAG-src.tar.xz.asc"
  ;;
esac
