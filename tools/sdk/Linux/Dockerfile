FROM centos:7

RUN yum -y update
RUN yum -y install epel-release gcc gcc-c++ wget flex bison rsync bzip2 make xz file

ADD gcc-deps.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD llvm.sh /image/llvm.sh
RUN /image/llvm.sh

ADD audio.sh /image/audio.sh
RUN /image/audio.sh

