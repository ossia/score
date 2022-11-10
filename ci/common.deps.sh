#!/bin/bash

(
ls
pwd
cd src/addons

if [[ ! -d iscore-addon-network ]]; then
  git clone --recursive -j16 https://github.com/ossia/iscore-addon-network
fi

if [[ ! -d score-addon-synthimi ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-synthimi
fi

if [[ ! -d score-addon-threedim ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-threedim
fi

if [[ ! -d score-addon-jk ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-jk
fi

if [[ ! -d score-addon-ndi ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-ndi
fi
)
