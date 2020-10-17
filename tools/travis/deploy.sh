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

    # Codesign
    security unlock-keychain -p travis build.keychain
    codesign \
      --entitlements /Users/travis/build/ossia/score/src/app/entitlements.plist \
      --force \
      --timestamp \
      --sign "ossia.io" \
      score.app

    # Create a .dmg
    brew install graphicsmagick imagemagick
    npm install --global create-dmg
    cp /Users/travis/build/ossia/score/LICENSE.txt license.txt
    create-dmg 'score.app'
    ls

    # Notarize the .dmg
    xcrun altool --notarize-app \
                 -t osx \
                 -f *.dmg \
                 --primary-bundle-id "io.ossia.score" \
                 -u jeanmichael.celerier@gmail.com \
                 -p "@env:MAC_ALTOOL_PASSWORD"

    mv *.dmg "/Users/travis/build/ossia/score/deploy/ossia score-$TAG-macOS.dmg"
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
