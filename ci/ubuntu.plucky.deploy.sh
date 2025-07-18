#!/bin/bash -eux
export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")

find . -name '*.deb'
mv *.deb "ossia score-$TAG-ubuntu2504-amd64.deb" || true
mv build/*.deb "ossia score-$TAG-ubuntu2504-amd64.deb" || true
