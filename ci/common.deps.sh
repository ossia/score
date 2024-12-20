#!/usr/bin/env bash

(
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

if [[ ! -d score-avnd-granola ]]; then
  git clone --recursive -j16 https://github.com/bltzr/score-avnd-granola
fi

if [[ ! -d score-addon-ultraleap ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-ultraleap
fi

if [[ ! -d score-addon-contextfree ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-contextfree
fi

if [[ ! -d score-addon-ble ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-ble
fi

if [[ ! -d score-addon-led ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-led
fi

if [[ ! -d score-addon-spatgris ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-spatgris
fi

if [[ ! -d score-addon-hdf5 ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-hdf5
fi

if [[ ! -d score-addon-puara ]]; then
  git clone --recursive -j16 https://github.com/ossia/score-addon-puara
fi

)


