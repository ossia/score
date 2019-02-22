#!/bin/sh
cd ..
rm -rf .git
rm -rf API/3rdparty/CicmWrapper
rm -rf API/3rdparty/pure-data
rm -rf API/3rdparty/pybind11
rm -rf API/3rdparty/jni_hpp
rm -rf API/3rdparty/max-sdk

tar caf ossia-score.tar.xz ./*
