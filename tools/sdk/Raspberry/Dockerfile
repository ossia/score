FROM  philipz/rpi-raspbian


COPY qemu-arm-static /usr/bin/qemu-arm-static
RUN apt -y update
RUN apt -y install build-essential git wget bzip2 bison flex texinfo debhelper chrpath texinfo sharutils libffi-dev patchutils diffstat xz-utils libedit-dev procps zlib1g-dev perl
ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD cmake.sh /image/cmake.sh
RUN /image/cmake.sh

ADD llvm.sh /image/llvm.sh
RUN /image/llvm.sh

ADD audio.sh /image/audio.sh
RUN /image/audio.sh


RUN rm -rf /image

