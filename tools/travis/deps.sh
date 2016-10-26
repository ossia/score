#!/bin/bash -eux

# export ISCORE_PLUGINS_TO_BUILD=("iscore-addon-csp")

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
    sudo apt-get install -qq g++-6 libasound-dev ninja-build gcovr lcov

    sudo wget https://sourceforge.net/projects/boost/files/boost/1.62.0/boost_1_62_0.tar.bz2 -O /opt/boost.tar.bz2
    (cd /opt; sudo tar xaf boost.tar.bz2; sudo mv boost_* boost ; sudo chmod -R a+rwx boost)

    if [[ "$CONF" == "linux-package" ]];
    then
      sudo wget https://www.dropbox.com/s/a8w8o3mu0jfr3t8/qt5.6-static-release.tar.xz?dl=1 -O /opt/qt5.6-static-release.tgz
      (cd /opt; sudo tar xaf qt5.6-static-release.tgz)
      sudo apt-get install -qq libxcb-icccm4-dev libxi-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-xkb-dev libxcb-render-util0-dev libxinerama-dev libxcb-xinerama0-dev libudev-dev
    else
      sudo apt-get install -qq qt57-meta-full
    fi

    wget https://cmake.org/files/v3.6/cmake-3.6.2-Linux-x86_64.tar.gz -O cmake-linux.tgz
    tar xaf cmake-linux.tgz
    mv cmake-*-x86_64 cmake
  ;;
  osx)
    set +e

    brew remove boost
    ARCHIVE=homebrew-cache.tar.xz
    brew install wget gnu-tar xz
    wget https://github.com/OSSIA/iscore-sdk/releases/download/6.0-osx/$ARCHIVE -O $ARCHIVE
    gtar xhaf $ARCHIVE --directory /usr/local/Cellar
    brew unlink cmake
    brew link --force boost cmake ninja qt5 wget
    
    set -e
  ;;
esac
