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
      sudo add-apt-repository --yes ppa:beineri/opt-qt56-trusty
    fi

    sudo apt-get update -qq
    sudo apt-get install -qq g++-5 libavahi-compat-libdnssd-dev libportmidi-dev libasound2-dev ninja-build gcovr lcov

    if [[ "$CONF" == "linux-package" ]];
    then
      sudo wget https://www.dropbox.com/s/xpj072x4tcf32gf/boost_1_60_0.tar.bz2?dl=1 -O /opt/boost_1_60_0.tar.bz2
      (cd /opt; sudo tar xaf boost_1_60_0.tar.bz2; sudo chmod -R a+rwx boost_1_60_0)
      sudo wget https://www.dropbox.com/s/a8w8o3mu0jfr3t8/qt5.6-static-release.tar.xz?dl=1 -O /opt/qt5.6-static-release.tgz
      (cd /opt; sudo tar xaf qt5.6-static-release.tgz)
      sudo apt-get install -qq libxcb-icccm4-dev libxi-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-xkb-dev libxcb-render-util0-dev libxinerama-dev libxcb-xinerama0-dev libudev-dev
    else
      sudo apt-get install -qq qt56-meta-full libboost1.55-dev
    fi

    wget https://github.com/OSSIA/iscore-sdk/releases/download/1.0/cmake-linux.tgz
    tar xaf cmake-linux.tgz
    mv cmake-3.5.2-Linux-x86_64 cmake

    wget https://www.dropbox.com/s/0pmy14zlpqpyaq6/JamomaCore-0.6-dev-Linux.deb?dl=1 -O jamoma.deb
    sudo dpkg -i jamoma.deb
  ;;
  osx)
    set +e

    brew install wget gnu-tar
    wget https://www.dropbox.com/s/ycl6tmct75po1n6/homebrew-cache.tar.gz?dl=1 -O homebrew-cache.tar.gz
    gtar xhzf homebrew-cache.tar.gz --directory /usr/local/Cellar
    brew link --force boost cmake ninja qt5 wget

    wget https://www.dropbox.com/s/t155m8wt2cp075k/JamomaDarwin20151108.zip?dl=1 -O JamomaDarwin20151108.zip
    unzip JamomaDarwin20151108.zip
    mv JamomaDarwin20151108 Jamoma
    set -e
  ;;
esac
