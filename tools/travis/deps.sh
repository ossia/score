#!/bin/bash -eux

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo wget https://github.com/OSSIA/score-sdk/releases/download/sdk7/boost.tar.bz2 -O /opt/boost.tar.bz2 &

    wget https://cmake.org/files/v3.8/cmake-3.8.2-Linux-x86_64.tar.gz -O cmake-linux.tgz &

    echo 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-5.0 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551

    sudo add-apt-repository --yes ppa:jonathonf/ffmpeg-3
    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt592-trusty

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes g++-7 binutils libasound-dev ninja-build gcovr lcov qt59-meta-minimal qt59svg qt59quickcontrols2 qt59websockets qt59serialport qt59multimedia libgl1-mesa-dev libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev portaudio19-dev clang-5.0 lld-5.0

    wait wget || true
    (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)

    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake
  ;;
  osx)
    set +e

    brew update
    ARCHIVE=homebrew-cache.tar.xz
    brew install gnu-tar xz

    wget -nv https://github.com/OSSIA/score-sdk/releases/download/sdk8/$ARCHIVE -O $ARCHIVE
    gtar xhaf $ARCHIVE --directory /usr/local/Cellar
    brew unlink cmake
    brew link --force boost ninja qt5
    brew install portaudio

    wget -nv https://cmake.org/files/v3.8/cmake-3.8.2-Darwin-x86_64.tar.gz
    gtar xaf cmake-3.8.2-Darwin-x86_64.tar.gz --directory /tmp
    set -e
  ;;
esac
