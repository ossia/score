#!/bin/bash -eux
export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")

mv "score.tar.gz" "ossia score-$TAG-linux-raspberrypi.tar.gz"
