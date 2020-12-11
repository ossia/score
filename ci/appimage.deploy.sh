#!/bin/bash -eux
export TAG=$GITTAGNOV

mv "Score.AppImage" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-linux-amd64.AppImage"
mv "linux-sdk.zip" "$BUILD_ARTIFACTSTAGINGDIRECTORY/"
