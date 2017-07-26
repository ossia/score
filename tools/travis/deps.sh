#!/bin/bash -eux

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo wget https://github.com/OSSIA/iscore-sdk/releases/download/sdk7/boost.tar.bz2 -O /opt/boost.tar.bz2 &
    
    wget https://cmake.org/files/v3.9/cmake-3.9.0-Linux-x86_64.tar.gz -O cmake-linux.tgz &
    
    echo 'deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-4.0 main' | sudo tee /etc/apt/sources.list.d/llvm.list
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551

    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
    sudo add-apt-repository --yes ppa:beineri/opt-qt591-trusty

    sudo apt-get update -qq
    sudo apt-get install -qq --force-yes g++-6 binutils libasound-dev ninja-build gcovr lcov qt59-meta-minimal qt59svg qt59quickcontrols2 qt59websockets qt59serialport qt59multimedia clang-4.0 libgl1-mesa-dev libavcodec-dev libavutil-dev libavfilter-dev libavformat-dev libavresample-dev
    # lld-4.0 : too buggy yet
    
    wait wget || true
    (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)

    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake
  ;;
  osx)
    set +e

    brew uninstall --ignore-dependencies boost
    ARCHIVE=homebrew-cache.tar.xz
    brew install gnu-tar xz

    wget https://github.com/OSSIA/iscore-sdk/releases/download/sdk7/$ARCHIVE -O $ARCHIVE
    gtar xhaf $ARCHIVE --directory /usr/local/Cellar
    brew unlink cmake
    brew link --force boost cmake ninja qt5
    
    set -e
  ;;
esac
