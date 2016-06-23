#!/bin/bash -eux

git submodule update --init --recursive
export ISCORE_PLUGINS_TO_BUILD=("iscore-addon-csp")

# In this case everything is built in docker
if [[ "$CONF" == "linux-package-appimage" ]];
then
    exit 0
fi

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 1397BC53640DB551
    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test

    if [[ "$CONF" != "linux-package" ]];
    then
      sudo add-apt-repository --yes ppa:beineri/opt-qt57-trusty
    fi

    sudo apt-get update -qq
    sudo apt-get install -qq g++-5 libavahi-compat-libdnssd-dev libportmidi-dev libasound2-dev ninja-build gcovr lcov

    sudo wget https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2 -O /opt/boost.tar.bz2
    (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)
    
    if [[ "$CONF" == "linux-package" ]];
    then
      sudo wget https://www.dropbox.com/s/a8w8o3mu0jfr3t8/qt5.6-static-release.tar.xz?dl=1 -O /opt/qt5.6-static-release.tgz
      (cd /opt; sudo tar xaf qt5.6-static-release.tgz)
      sudo apt-get install -qq libxcb-icccm4-dev libxi-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-xkb-dev libxcb-render-util0-dev libxinerama-dev libxcb-xinerama0-dev libudev-dev
    else
      sudo apt-get install -qq qt57-meta
    fi

    wget https://cmake.org/files/v3.6/cmake-3.6.0-rc1-Linux-x86_64.tar.gz -O cmake-linux.tgz
    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake

    wget https://www.dropbox.com/s/0pmy14zlpqpyaq6/JamomaCore-0.6-dev-Linux.deb?dl=1 -O jamoma.deb
    sudo dpkg -i jamoma.deb
  ;;
  osx)
    set +e

    brew install wget gnu-tar
    wget https://github.com/OSSIA/iscore-sdk/releases/download/2.0-OSX/homebrew-cache.tar.gz -O homebrew-cache.tar.gz
    gtar xhzf homebrew-cache.tar.gz --directory /usr/local/Cellar
    brew link --force boost cmake ninja qt5 wget

    wget https://github.com/OSSIA/iscore-sdk/releases/download/2.0-OSX/JamomaDarwin-2016-06-12.tar.gz -O Jamoma.tar.gz
    gtar xhzf Jamoma.tar.gz
    set -e
  ;;
esac
