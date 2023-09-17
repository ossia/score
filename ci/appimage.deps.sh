#!/bin/bash -eux

sudo apt-get update -qq
sudo apt-get install wget libfuse2

source ci/common.deps.sh
