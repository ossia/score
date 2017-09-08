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
    mv "score.AppImage" "score-$TRAVIS_TAG-amd64.AppImage"
else
    cd bundle
    zip -r -9 "score-$TRAVIS_TAG-OSX.zip" score.app
    mv "score-$TRAVIS_TAG-OSX.zip" ..
fi

