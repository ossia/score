#!/bin/bash
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
#~    if [[ -f /etc/lsb-release ]]; then
#~        LSB_ID=$(cat /etc/lsb-release | grep DISTRIB_ID | awk -F= "{ print $2 }" | tr "\n" " " | sed "s/ //" | cut -d'=' -f2)
#~        LSB_VER=$(cat /etc/lsb-release | grep DISTRIB_RELEASE | awk -F= "{ print $2 }" | tr "\n" " " | sed "s/ //" | cut -d'=' -f2)
#~
#~        GOOD_FILENAME=$(find . -maxdepth 1 -type f -name "i-score-*-$LSB_ID-$LSB_VER.*")
#~    else
#~        GOOD_FILENAME=i-score-0.3.0-linux-generic.tar.gz
#~    fi

else
    zip -r -9 "i-score-$TRAVIS_TAG-OSX.zip" bundle/i-score.app
fi

