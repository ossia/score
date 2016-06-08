FROM centos:6

RUN yum -y update 
RUN yum -y install epel-release gcc gcc-c++ wget

ADD gcc.sh /image/gcc-deps.sh
RUN /image/gcc-deps.sh

ADD gcc.sh /image/gcc.sh
RUN /image/gcc.sh

ADD qt.sh /image/qt.sh
