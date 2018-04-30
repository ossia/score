#!/bin/bash
yum install -y nasm
wget -nv https://ffmpeg.org/releases/ffmpeg-4.0.tar.bz2
tar -xaf ffmpeg-4.0.tar.bz2
cd ffmpeg-4.0
./configure --arch=x86_64 --cpu=x86_64 --disable-doc --disable-ffmpeg --disable-ffplay --disable-debug --prefix=/usr/local --pkg-config-flags="--static" --enable-gpl --enable-version3 --disable-openssl --disable-securetransport --disable-videotoolbox --disable-network --disable-iconv --disable-lzma
make -j8
make install

yum -y install cmake alsa-lib-devel
wget -nv http://www.portaudio.com/archives/pa_snapshot.tgz
tar -xaf pa_snapshot.tgz
cd portaudio/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j8
make install
cp libportaudio_static.a /usr/local/lib
cp libportaudio_static.a /usr/local/lib64

# jack-audio-connection-kit-devel
