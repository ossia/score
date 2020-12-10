#!/bin/bash -eux
if [[ "$GITTAG" = "" ]]; then
    GITTAG=devel
fi

export TAG=$(echo $GITTAG | tr -d v)

mkdir deploy
mv "Score.AppImage" "deploy/ossia score-$TAG-linux-amd64.AppImage"
mv "linux-sdk.zip" "deploy/"
