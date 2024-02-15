#!/bin/bash -eux

sudo apt-get update -qq
sudo apt-get install wget libfuse2 desktop-file-utils

source ci/common.deps.sh
