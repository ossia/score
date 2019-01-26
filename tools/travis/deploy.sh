#!/bin/bash -eux
if [[ "$TRAVIS_TAG" = "" ]];
then
    exit 0
fi

cd build

case "$CONF" in
  osx-package)
    cd bundle
    zip -r -9 "Score-$TRAVIS_TAG-macOS.zip" Score.app
    mv "Score-$TRAVIS_TAG-macOS.zip" ..
  ;;
  linux-package-appimage)
    mv "Score.AppImage" "Score-$TRAVIS_TAG-amd64.AppImage"
  ;;
  tarball)
    mv "ossia-score.tar.xz" "Score-$TRAVIS_TAG-src.tar.xz"
  ;;
esac
