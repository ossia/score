#!/bin/bash

source ci/common.deps.sh

printf "r%s.%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)" > COMMIT
(
  rm -rf .git
  rm -rf *.tar.xz
  find . -name '.git' -exec rm -rf {} \; || true
  find . -name '.git' -exec rm -rf {} \; || true
  rm -rf 3rdparty/libossia/.git || true
  rm -rf 3rdparty/libossia/3rdparty/CicmWrapper || true
  
  cp -rf 3rdparty/libpd/pure-data/LICENSE.txt pd-license.txt || true
  cp -rf 3rdparty/libpd/pure-data/portaudio/portaudio/LICENSE.txt portaudio-license.txt || true
  rm -rf 3rdparty/libossia/3rdparty/pure-data || true
  mkdir -p 3rdparty/libossia/3rdparty/pure-data/portaudio/portaudio/ || true
  mv portaudio-license.txt 3rdparty/libossia/3rdparty/pure-data/portaudio/portaudio/LICENSE.txt  || true
  mv pd-license.txt 3rdparty/libossia/3rdparty/pure-data/LICENSE.txt  || true
  
  mv 3rdparty/libossia/3rdparty/pybind11/LICENSE pybind11-license.txt || true
  rm -rf 3rdparty/libossia/3rdparty/pybind11 || true
  mkdir -p 3rdparty/libossia/3rdparty/pybind11 || true
  mv pybind11-license.txt 3rdparty/libossia/3rdparty/pybind11/LICENSE || true
  
  rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/benchmarks
  rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/build
  rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/test
  rm -rf 3rdparty/libossia/3rdparty/r8brain-free-src/bench
  rm -rf 3rdparty/libossia/3rdparty/r8brain-free-src/DLL/Win32
  rm -rf 3rdparty/libossia/3rdparty/r8brain-free-src/DLL/Win64
  rm -rf 3rdparty/libossia/3rdparty/jni_hpp
  rm -rf 3rdparty/libossia/3rdparty/max-sdk
  rm -rf 3rdparty/libossia/3rdparty/libremidi/tests/corpus
  rm -rf 3rdparty/libossia/3rdparty/kfr/tests
  rm -rf 3rdparty/libossia/3rdparty/kfr/docs
  rm -rf 3rdparty/libossia/3rdparty/kfr/img
  rm -rf 3rdparty/libossia/3rdparty/rapidjson/thirdparty
  rm -rf 3rdparty/libossia/3rdparty/dr_libs/tests
  rm -rf 3rdparty/libossia/3rdparty/unordered_dense/data/fuzz
  rm -rf 3rdparty/libossia/3rdparty/exprtk/*test*
  rm -rf 3rdparty/libossia/3rdparty/purr-data
  rm -rf 3rdparty/libossia/3rdparty/websocketpp/test
  rm -rf 3rdparty/libossia/3rdparty/boost*

  rm -rf 3rdparty/libossia/src/ossia-max
  rm -rf 3rdparty/libossia/src/ossia-pd
  rm -rf 3rdparty/libossia/src/ossia-python
  rm -rf 3rdparty/libossia/src/ossia-java

  rm -rf 3rdparty/outcome/doc
  rm -rf 3rdparty/outcome/benchmark
  rm -rf 3rdparty/outcome/test
  rm -rf 3rdparty/llfio/doc
  rm -rf 3rdparty/llfio/reference
  rm -rf 3rdparty/llfio/attic
  rm -rf 3rdparty/vst3/public.sdk/samples
  rm -rf 3rdparty/libpd/pure-data/doc
  rm -rf 3rdparty/libpd/pure-data/msw
  rm -rf 3rdparty/libpd/samples
  rm -rf 3rdparty/quickcpplib/doc
  rm -rf 3rdparty/quickcpplib/pcpp
  rm -rf 3rdparty/shmdata/doc
  rm -rf 3rdparty/sh4lt/doc
  rm -rf 3rdparty/snappy/third_party
  rm -rf 3rdparty/snappy/testdata
  rm -rf 3rdparty/DSPFilters/DSPFiltersDemo
  rm -rf 3rdparty/DSPFilters/doc
  rm -rf docs/Doc_sources
  rm -rf docs/Doxygen
  rm -rf docs/score.png

  rm -rf 3rdparty/vcglib/apps
  rm -rf 3rdparty/eigen/doc
  rm -rf 3rdparty/eigen/test
  rm -rf 3rdparty/eigen/bench

  rm -rf src/addons/score-addon-airwindows/3rdparty/airwin2rack/libs/airwindows/plugins

  rm -rf src/addons/score-addon-onnx/3rdparty/onnxruntime-extensions/test

  rm -rf src/addons/score-addon-hdf5/3rdparty/hdf5/tools
  rm -rf src/addons/score-addon-hdf5/3rdparty/hdf5/test
  rm -rf src/addons/score-addon-hdf5/3rdparty/hdf5/doxygen
  rm -rf src/addons/score-addon-hdf5/3rdparty/hdf5/HDF5Examples

  rm -rf src/addons/score-addon-puara/3rdparty/puara-gestures/3rdparty/boost

  rm -rf cmake-build-*
  find . -name '*.user' -exec rm {} \;
  find . -name '*.user.*' -exec rm {} \;
)

if [[ -v GPG_SIGN_PUBKEY ]]; then
  export PUBKEY_SECUREFILEPATH=gpg.pub.asc
  export PRIVKEY_SECUREFILEPATH=gpg.priv.asc
  echo "$GPG_SIGN_PUBKEY" > "$PUBKEY_SECUREFILEPATH"
  echo "$GPG_SIGN_PRIVKEY" > "$PRIVKEY_SECUREFILEPATH"
fi

tar caf ossia-score.tar.xz --transform "s,^\.\/,ossia-score-$GITTAGNOV/," ./*
gpg --import "$PUBKEY_SECUREFILEPATH"
gpg --allow-secret-key-import --import "$PRIVKEY_SECUREFILEPATH"
gpg -ab ossia-score.tar.xz
