FROM ubuntu:14.04

RUN apt -y update
RUN apt -y --force-yes dist-upgrade
RUN apt-get -y --force-yes install build-essential git wget bzip2 bison flex texinfo 

ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt-deps.sh /image/qt-deps.sh
RUN /image/qt-deps.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

#ADD cmake.sh /image/cmake.sh
#RUN /image/cmake.sh

