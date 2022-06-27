#!/bin/bash -eux
export TAG=$(echo "$GITHUB_REF" | sed "s/.*\///;s/^v//")

mv "*.deb" "ossia score-$TAG-ubuntu22-amd64.deb"
