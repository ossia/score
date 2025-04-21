#!/bin/bash -eux
export TAG="$GITTAGNOV"

mv "Score.AppImage" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-linux-$CPU_ARCH.AppImage"
mv "linux-sdk.zip" "$BUILD_ARTIFACTSTAGINGDIRECTORY/sdk-linux-$CPU_ARCH.zip"
tar caf "$BUILD_ARTIFACTSTAGINGDIRECTORY/score-bin.tar.gz" /tmp/build/ossia-score
