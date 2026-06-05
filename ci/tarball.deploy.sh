#!/bin/bash -eux

export TAG=$GITTAGNOV

mv "ossia-score.tar.xz" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-src.tar.xz"

if [[ -f "ossia-score.tar.xz.asc" ]]; then
  mv "ossia-score.tar.xz.asc" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-src.tar.xz.asc"
fi
