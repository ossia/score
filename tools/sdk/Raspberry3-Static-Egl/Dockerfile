FROM  philipz/rpi-raspbian


COPY qemu-arm-static /usr/bin/qemu-arm-static
COPY sources.list /etc/apt/sources.list
RUN apt -y update
RUN apt -y --force-yes dist-upgrade
RUN apt-get -y --force-yes install build-essential git wget bzip2 bison flex texinfo "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev flex bison gperf libicu-dev libxslt-dev ruby perl python  libasound2-dev libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev libfontconfig1-dev libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev libdbus-1-dev openssl libraspberrypi0 libraspberrypi-dev libraspberrypi-doc libraspberrypi-bin libdbus-1-dev libgstreamer1.0-dev libopenal-dev libbluetooth-dev libsdl2-dev libinput-dev libts-dev libmtdev-dev libfontconfig1-dev libsctp-dev libmysqlclient-dev libgstreamer-plugins-base1.0-dev libssl*-dev
RUN rsync -a /opt/vc/ /usr/

ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt-deps.sh /image/qt-deps.sh
RUN /image/qt-deps.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD cmake.sh /image/cmake.sh
RUN /image/cmake.sh

