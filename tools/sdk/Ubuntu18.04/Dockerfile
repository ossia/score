FROM ubuntu:18.04

RUN apt -y update
RUN apt -y --force-yes dist-upgrade
RUN apt -y --force-yes install git cmake wget ninja-build libqt5websockets5-dev qtbase5-dev qtdeclarative5-dev qt5-default qtbase5-dev-tools qttools5-dev libqt5svg5-dev build-essential g++ libportmidi0 libasound-dev mesa-common-dev libavahi-compat-libdnssd-dev liblilv-dev libsuil-dev lilv-utils libboost-dev qtquickcontrols2-5-dev libqt5serialport5-dev  libavcodec-dev libavformat-dev libavfilter-dev libavdevice-dev  libqt5serialport5-dev libz-dev libtbb-dev

RUN git clone --recursive -j16 --depth=1 https://github.com/OSSIA/score

RUN mkdir build ; cd build ; cmake ../score -DSCORE_CONFIGURATION=static-release ; cmake --build . --target all_unity -- -j16 ; cpack -G DEB

