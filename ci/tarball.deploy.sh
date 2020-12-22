#!/bin/bash -eux

export TAG=$GITTAGNOV

mv "ossia-score.tar.xz" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-src.tar.xz"
mv "ossia-score.tar.xz.asc" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-src.tar.xz.asc"
