FROM  philipz/rpi-raspbian


COPY qemu-arm-static /usr/bin/qemu-arm-static
RUN apt -y update
RUN apt -y install build-essential git wget bzip2 bison flex texinfo

ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD cmake.sh /image/cmake.sh
RUN /image/cmake.sh

RUN rm -rf /image

