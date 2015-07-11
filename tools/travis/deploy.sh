#!/bin/bash
if [[ "$TRAVIS_BRANCH" != "dev" ]];
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
  zip -r -9 i-score-0.3.0-OSX.zip bundle/i-score.app
	GOOD_FILENAME=i-score-0.3.0-OSX.zip
	DISTRO=i-score_osx
fi
curl -T build/i-score/$GOOD_FILENAME -u$BINTRAY_USER:$BINTRAY_API_KEY https://api.bintray.com/content/ossia/i-score/$DISTRO/0.3/$GOOD_FILENAME
curl -XPOST -u$BINTRAY_USER:$BINTRAY_API_KEY https://api.bintray.com/content/ossia/i-score/$DISTRO/0.3/publish
curl -XGET https://bintray.com/ossia/i-score/$DISTRO/0.3
