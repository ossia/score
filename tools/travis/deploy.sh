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
    mv "i-score.AppImage" "i-score-$TRAVIS_TAG-amd64.AppImage"
else
    zip -r -9 "i-score-$TRAVIS_TAG-OSX.zip" bundle/i-score.app
fi

