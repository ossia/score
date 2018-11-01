FROM centos:7

RUN yum -y update
RUN yum -y install epel-release gcc gcc-c++ wget flex bison rsync bzip2 make xz file \
           svn perl-Data-Dump perl-Data-Dumper \
           perl harfbuzz-devel which make perl-version libxcb libxcb-devel xcb-util xcb-util-devel fontconfig-devel libX11-devel libXrender-devel libXi-devel git dbus-devel glib2-devel mesa-libGL-devel openssl-devel

# ADD gcc-deps.sh /image/gcc-deps.sh
# RUN /image/gcc-deps.sh
# 
# ADD gcc.sh /image/gcc.sh
# RUN /image/gcc.sh
# 
ADD llvm-deps.sh /image/llvm-deps.sh
RUN /image/llvm-deps.sh

ADD llvm.sh /image/llvm.sh
RUN /image/llvm.sh

ADD qt.sh /image/qt.sh
RUN /image/qt.sh

ADD audio.sh /image/audio.sh
RUN /image/audio.sh

