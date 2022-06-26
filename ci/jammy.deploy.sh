#!/bin/bash -eux
export TAG=$GITTAGNOV

mv "*.deb" "$BUILD_ARTIFACTSTAGINGDIRECTORY/ossia score-$TAG-ubuntu22-amd64.deb"
