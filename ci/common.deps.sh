#!/bin/bash

(
ls
pwd
cd src/addons

git clone --recursive -j16 https://github.com/ossia/iscore-addon-network
git clone --recursive -j16 https://github.com/ossia/score-addon-synthimi
)
