#!/bin/sh
git submodule init

case "$TRAVIS_OS_NAME" in
    linux)
        sudo add-apt-repository --yes ppa:ubuntu-toolchain-r/test
        sudo add-apt-repository --yes ppa:beineri/opt-qt541
        sudo add-apt-repository --yes ppa:boost-latest/ppa

        sudo apt-get update -qq
        sudo apt-get install -qq qt54-meta-full libboost1.55-dev libavahi-compat-libdnssd-dev

        wget https://www.dropbox.com/s/3xot58gakn6w898/cmake_3.2.3-3.2.3_amd64.deb?dl=1 -O cmake_3.2.3-3.2.3_amd64.deb
        sudo dpkg --force-overwrite -i cmake_3.2.3-3.2.3_amd64.deb

        wget https://www.dropbox.com/s/hy7zf5iv2cohq2d/gcc_5.1.0-1_amd64.deb?dl=1 -O gcc_5.1.0-1_amd64.deb
        sudo dpkg --force-overwrite -i  gcc_5.1.0-1_amd64.deb

	;;
	osx)
	    brew tap ossia/taps
	    brew install jamomacore --HEAD
		brew install cmake qt5 boost
	;;
esac