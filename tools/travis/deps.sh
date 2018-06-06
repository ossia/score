#!/bin/bash -eux

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo wget https://github.com/OSSIA/score-sdk/releases/download/sdk8/boost.tar.bz2 -O /opt/boost.tar.bz2 &

    wget -nv https://cmake.org/files/v3.11/cmake-3.11.3-Linux-x86_64.tar.gz -O cmake-linux.tgz &

    echo 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-6.0 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551

    sudo add-apt-repository --yes ppa:jonathonf/ffmpeg-3
    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt-5.10.1-trusty

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes g++-7 binutils libasound-dev ninja-build gcovr lcov qt510-meta-minimal qt510svg qt510quickcontrols2 qt510websockets qt510serialport qt510multimedia libgl1-mesa-dev libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libswresample-dev portaudio19-dev clang-6.0 lld-6.0

    wait wget || true
    (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)

    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake
  ;;
  osx)
    set +e

    brew update
    brew install gnu-tar xz

    SDK_ARCHIVE=homebrew-cache.txz
    wget -nv https://github.com/OSSIA/score-sdk/releases/download/sdk9/$SDK_ARCHIVE -O $SDK_ARCHIVE
    gtar xhaf $SDK_ARCHIVE --directory /usr/local/Cellar

    AUDIO_ARCHIVE=audio-libs.txz
    wget -nv https://github.com/OSSIA/score-sdk/releases/download/sdk8/$AUDIO_ARCHIVE -O $AUDIO_ARCHIVE
    sudo gtar xhaf $AUDIO_ARCHIVE --directory /opt

    brew unlink cmake
    brew link --force boost ninja qt5 cmake
    brew install portaudio

    set -e
  ;;
esac
