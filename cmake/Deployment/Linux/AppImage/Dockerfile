FROM centos:7

RUN yum -y update && \
    yum -y install epel-release centos-release-scl-rh && \
    yum -y update && \
    yum -y install ninja-build jack-audio-connection-kit-devel libmpc \
           sudo wget tar bzip2 xz git libxml2  binutils fuse mesa-libGL-devel \
           glibc-devel glib2-devel fuse-devel zlib-devel libpng12 libXrender \
           fontconfig freetype xcb-util xcb-util-image xcb-util-keysyms \
           xcb-util-renderutil xcb-util-wm libXi alsa-lib-devel git wget \
           make binutils libarchive3-devel patch openssl-static openssl-devel vim-common python-devel ncurses-devel ncurses-libs ncurses \
           bluez-libs-devel

ADD Recipe.deps /Recipe.deps
RUN bash -ex Recipe.deps

# ADD Recipe /Recipe
# RUN bash -ex Recipe
