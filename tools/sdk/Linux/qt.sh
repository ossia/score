#!/bin/bash -eux

cd /image

NPROC=$(nproc)

yum install perl-version libxcb libxcb-devel xcb-util xcb-util-devel fontconfig-devel libX11-devel libXrender-devel libXi-devel git

git clone https://github.com/qtproject/qt5
cd qt5
perl init-repository --module-subset=default,-qtwebkit,-qtwebkit-examples,-qtwebengine
