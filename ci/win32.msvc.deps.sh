#!/bin/bash

choco install -y ninja
choco install -y rsync

(
set -x
mkdir /c/ossia-sdk-msvc
cd /c/ossia-sdk-msvc
curl -L https://github.com/ossia/sdk/releases/download/sdk25/sdk-msvc.7z --output sdk-msvc.7z
7z x sdk-msvc.7z
rm sdk-msvc.7z

curl -L https://github.com/ossia/sdk/releases/download/sdk31/boost_1_90_0.tar.gz --output boost.tar.gz
tar -xzf boost.tar.gz
rm boost.tar.gz
mv boost_1_90_0 boost

ls
)

source ci/common.deps.sh
