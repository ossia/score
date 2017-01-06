FROM  philipz/rpi-raspbian


COPY qemu-arm-static /usr/bin/qemu-arm-static
COPY sources.list /etc/apt/sources.list
RUN apt -y update
RUN apt -y --force-yes install build-essential git wget bzip2 bison flex texinfo "^libxcb.*" libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev flex bison gperf libicu-dev libxslt-dev ruby perl python  libasound2-dev libgstreamer0.10-dev libgstreamer-plugins-base0.10-dev libx11-xcb-dev libglu1-mesa-dev libxrender-dev libxi-dev libfontconfig1-dev libfreetype6-dev libx11-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libx11-xcb-dev libxcb-glx0-dev libdbus-1-dev libssl-dev openssl libraspberrypi0 libraspberrypi-dev libraspberrypi-doc libraspberrypi-bin libdbus-1-dev

ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD cmake.sh /image/cmake.sh
RUN /image/cmake.sh

RUN rm -rf /image

