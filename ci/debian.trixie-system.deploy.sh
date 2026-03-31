#!/bin/bash -eux
export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")
export ARCH=$(dpkg --print-architecture)

find . -name '*.deb'
mv *.deb "ossia score-$TAG-debian13-system-$ARCH.deb" || true
