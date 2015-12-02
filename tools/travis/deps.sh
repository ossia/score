#!/bin/bash -eux

git submodule init

case "$TRAVIS_OS_NAME" in
  linux)
    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test

    if [[ "$STATIC_QT" = "False" ]];
    then
      sudo add-apt-repository --yes ppa:beineri/opt-qt55-trusty
    fi

    sudo apt-get update -qq
    sudo apt-get install -qq g++-5 libboost1.55-dev libavahi-compat-libdnssd-dev libportmidi-dev ninja-build gcovr lcov

    if [[ "$STATIC_QT" = "True" ]];
    then
      sudo wget https://www.dropbox.com/s/vjh9lm1n3sody2c/qt5-static-linux-release.tar.xz?dl=1 -O /opt/qt5-static-linux-release.tar.xz
      (cd /opt; sudo tar xaf qt5-static-linux-release.tar.xz)
      sudo apt-get install -qq libxcb-icccm4-dev libxi-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-xkb-dev libxcb-render-util0-dev
    else
      sudo apt-get install -qq qt55-meta-full
    fi

    wget https://www.dropbox.com/s/3xot58gakn6w898/cmake_3.2.3-3.2.3_amd64.deb?dl=1 -O cmake_3.2.3-3.2.3_amd64.deb
    sudo dpkg --force-overwrite -i cmake_3.2.3-3.2.3_amd64.deb

    wget https://www.dropbox.com/s/0pmy14zlpqpyaq6/JamomaCore-0.6-dev-Linux.deb?dl=1 -O jamoma.deb
    sudo dpkg -i jamoma.deb

    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 1000
  ;;
  osx)
    # work around a homebrew bug
    set +e
    brew update; brew update
    brew install wget
    wget https://www.dropbox.com/s/t155m8wt2cp075k/JamomaDarwin20151108.zip?dl=1 -O JamomaDarwin20151108.zip
    unzip JamomaDarwin20151108.zip
    mv JamomaDarwin20151108 Jamoma
    brew install cmake qt5 boost ninja
    set -e
  ;;
esac
