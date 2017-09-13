#!/bin/bash -eux
if [[ "$TRAVIS_TAG" = "" ]];
then
    exit 0
fi

if [[ "$CAN_DEPLOY" = "False" ]];
then
    exit 0
fi

cd build
if [[ "$TRAVIS_OS_NAME" = "linux" ]]; then
    mv "Score.AppImage" "Score-$TRAVIS_TAG-amd64.AppImage"
else
    cd bundle
    zip -r -9 "Score-$TRAVIS_TAG-OSX.zip" Score.app
    mv "Score-$TRAVIS_TAG-OSX.zip" ..
fi

