#!/bin/bash -eux

git submodule update --init --recursive
export ISCORE_PLUGINS_TO_BUILD=("OSSIA/iscore-addon-csp")

# Set-up the plug-ins
(
cd base/addons
for plugin in "${ISCORE_PLUGINS_TO_BUILD[@]}"
do
  (
    git clone "https://github.com/$plugin"

    cd $plugin
    git submodule update --init --recursive
  )
done
)

# Install the deps
case "$TRAVIS_OS_NAME" in
  linux)
    sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test

    if [[ "$CONF" != "linux-package" ]];
    then
      sudo add-apt-repository --yes ppa:beineri/opt-qt551-trusty
    fi

    sudo apt-get update -qq
    sudo apt-get install -qq g++-5 libboost1.55-dev libavahi-compat-libdnssd-dev libportmidi-dev ninja-build gcovr lcov

    if [[ "$CONF" == "linux-package" ]];
    then
      sudo wget https://www.dropbox.com/s/vjh9lm1n3sody2c/qt5-static-linux-release.tar.xz?dl=1 -O /opt/qt5-static-linux-release.tar.xz
      (cd /opt; sudo tar xaf qt5-static-linux-release.tar.xz)
      sudo apt-get install -qq libxcb-icccm4-dev libxi-dev libxcb-image0-dev libxcb-keysyms1-dev libxcb-xkb-dev libxcb-render-util0-dev
    else
      sudo apt-get install -qq qt55-meta-full
    fi

#   wget https://www.dropbox.com/s/fiujf6l95g9nrjl/gcc5.3.deb?dl=1 -O gcc.deb
#   sudo dpkg --force-overwrite -i gcc.deb

    wget https://www.dropbox.com/s/ysnozd2sqre7x2d/cmake-3.4.1-Linux-x86_64.deb?dl=1 -O cmake.deb
    sudo dpkg --force-overwrite -i cmake.deb

    wget https://www.dropbox.com/s/0pmy14zlpqpyaq6/JamomaCore-0.6-dev-Linux.deb?dl=1 -O jamoma.deb
    sudo dpkg -i jamoma.deb

    # sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 1000
  ;;
  osx)
    set +e


    brew install wget gnu-tar
    wget https://www.dropbox.com/s/ycl6tmct75po1n6/homebrew-cache.tar.gz?dl=1 -O homebrew-cache.tar.gz
    gtar xhzf homebrew-cache.tar.gz --directory /usr/local/Cellar
    brew link --force boost cmake ninja qt5 wget

    # ./tools/travis/dl-homebrew.sh

#     if [ ! -f ~/travis-cache/homebrew-cache.tar.gz ]; then
 #      brew update
  #     brew install wget
   #    brew install cmake qt5 ninja
  #     brew upgrade boost
 #    fi

#     ./tools/travis/cache-homebrew.sh

    wget https://www.dropbox.com/s/t155m8wt2cp075k/JamomaDarwin20151108.zip?dl=1 -O JamomaDarwin20151108.zip
    unzip JamomaDarwin20151108.zip
    mv JamomaDarwin20151108 Jamoma
    set -e
  ;;
esac
