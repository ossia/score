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
    if [[ -f /etc/lsb-release ]]; then
        LSB_ID=$(cat /etc/lsb-release | grep DISTRIB_ID | awk -F= "{ print $2 }" | tr "\n" " " | sed "s/ //" | cut -d'=' -f2)
        LSB_VER=$(cat /etc/lsb-release | grep DISTRIB_RELEASE | awk -F= "{ print $2 }" | tr "\n" " " | sed "s/ //" | cut -d'=' -f2)
        FILENAME=i-score-0.3.0-$LSB_ID-$LSB_VER
        GOOD_FILENAME=$(find . -maxdepth 1 -type f -name "$FILENAME.*")
    else
        GOOD_FILENAME=i-score-0.3.0-linux-generic.tar.gz
    fi

    DISTRO=i-score_ubuntu12.04
    
else
    GOOD_FILENAME=i-score-0.3.0-OSX.zip
    zip -r -9 $GOOD_FILENAME bundle/i-score.app
    DISTRO=i-score_osx
fi

BETTER_FILENAME=$(echo "$GOOD_FILENAME"|sed "s/0.3.0/$TRAVIS_TAG/")
mv "$GOOD_FILENAME" "$BETTER_FILENAME"
pwd
find . -name "$BETTER_FILENAME"
