#!/bin/bash
cd ..
rm -rf .git
rm -rf *.tar.xz
find . -name '.git' -exec rm -rf {} \;
rm -rf 3rdparty/libossia/.git
rm -rf 3rdparty/libossia/3rdparty/CicmWrapper

cp 3rdparty/libossia/3rdparty/pure-data/LICENSE.txt pd-license.txt
cp 3rdparty/libossia/3rdparty/pure-data/portaudio/portaudio/LICENSE.txt portaudio-license.txt
rm -rf 3rdparty/libossia/3rdparty/pure-data
mkdir -p 3rdparty/libossia/3rdparty/pure-data/portaudio/portaudio/
mv portaudio-license.txt 3rdparty/libossia/3rdparty/pure-data/portaudio/portaudio/LICENSE.txt 
mv pd-license.txt 3rdparty/libossia/3rdparty/pure-data/LICENSE.txt 

mv 3rdparty/libossia/3rdparty/pybind11/LICENSE pybind11-license.txt
rm -rf 3rdparty/libossia/3rdparty/pybind11
mkdir -p 3rdparty/libossia/3rdparty/pybind11
mv pybind11-license.txt 3rdparty/libossia/3rdparty/pybind11/LICENSE

rm -rf 3rdparty/libossia/3rdparty/jni_hpp
rm -rf 3rdparty/libossia/3rdparty/max-sdk
rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/benchmarks
rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/build
rm -rf 3rdparty/libossia/3rdparty/concurrentqueue/test
rm -rf 3rdparty/libossia/3rdparty/tbb/examples
rm -rf 3rdparty/libossia/3rdparty/exprtk/*test*
rm -rf 3rdparty/libossia/3rdparty/purr-data
rm -rf 3rdparty/libossia/3rdparty/boost*
rm -rf docs/Doc_sources
rm -rf docs/Doxygen
rm -rf docs/score.png
rm -rf cmake-build-*
find . -name '*.user' -exec rm {} \;
find . -name '*.user.*' -exec rm {} \;

tar caf ossia-score.tar.xz ./*

#gpg --default-key ... -ab ossia-score.tar.xz 
