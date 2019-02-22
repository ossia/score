FROM centos:7
RUN yum -y install epel-release centos-release-scl  
RUN yum -y install devtoolset-7-gcc devtoolset-7-make devtoolset-7 \
           glibc-devel alsa-lib-devel mesa-libGL-devel libxkbcommon-x11-devel zlib-devel ncurses-devel \
           wget xz rh-git29 cmake3 

RUN mkdir -p /opt/score-sdk 
# WORKDIR /opt/score-sdk
RUN wget https://github.com/OSSIA/sdk/releases/download/sdk12/score-sdk-linux-llvm.tar.xz && \
    tar xaf score-sdk-linux-llvm.tar.xz && \
    rm -rf score-sdk-linux-llvm.tar.xz
    
#ADD Recipe.llvm /Recipe.llvm
#RUN bash -ex Recipe.llvm
