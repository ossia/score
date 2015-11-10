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
    RES=$(ls *.deb)
    mv "$RES" "i-score-$TRAVIS_TAG-Ubuntu-14.04-amd64.deb"
else
    zip -r -9 "i-score-$TRAVIS_TAG-OSX.zip" bundle/i-score.app
fi

