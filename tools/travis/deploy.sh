#!/bin/bash -eux
if [[ "$TRAVIS_TAG" = "" ]];
then
    exit 0
fi



case "$CONF" in
  osx-package)
    cd /Users/travis/build/ossia/score/bundle/
    zip -r -9 "Score-$TRAVIS_TAG-macOS.zip" score.app
    mv "Score-$TRAVIS_TAG-macOS.zip" ..
  ;;
  linux-package-appimage)
    cd /home/travis/build/ossia/score/build/
    mv "Score.AppImage" "Score-$TRAVIS_TAG-amd64.AppImage"
  ;;
  tarball)
    mv "ossia-score.tar.xz" "Score-$TRAVIS_TAG-src.tar.xz"
  ;;
esac
