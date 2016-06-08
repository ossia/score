#!/bin/bash -eux

cd /image

NPROC=$(nproc)

yum install perl-version libxcb libxcb-devel xcb-util xcb-util-devel fontconfig-devel libX11-devel libXrender-devel libXi-devel git

git clone https://github.com/qtproject/qt5
cd qt5
git checkout 5.6.1
perl init-repository --module-subset=default,-qtwebkit,-qtwebkit-examples,-qtwebengine,-qt3d,-qtcanvas3d,-qtactiveqt,-qtandroidextras,-qtcharts,-qtconnectivity,-qtdatavis3d,-qtenginio,-qtgamepad,-qtgraphicaleffects,-qtlocation,-qtmacextras,-qtpurchasing,-qtqa,-qtscxml,-qtscript,-qtsensors,-qtserialport,-qtvirtualkeyboard,-qtwayland,-qtwebchannel,-qtwebview,-qtwinextras,-qtx11extras,-qtxmlpatterns
