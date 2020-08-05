#!/bin/bash
apt -yy update && apt -yy full-upgrade

apt -yy install cmake git build-essential libsdl2-dev cmake qt5-default qtbase5-dev qtbase5-dev-tools qt5-image-formats-plugins qtdeclarative5-dev qtdeclarative5-dev-tools qttools5-dev qttools5-dev-tools g++-8 libavahi-compat-libdnssd-dev libavahi-client-dev libavahi-core-dev liblilv-dev libsuil-dev libjack-jackd2-dev libavcodec-dev libavdevice-dev  libavfilter-dev libavformat-dev libavutil-dev libswresample-dev libswscale-dev libbluetooth-dev libqt5websockets5-dev libqt5serialport5-dev libqt5svg5-dev qtquickcontrols2-5-dev ninja-build wget

wget -nv http://www.portaudio.com/archives/pa_snapshot.tgz
tar xaf pa_snapshot.tgz

sed -i '378i TARGET_INCLUDE_DIRECTORIES(portaudio_static PUBLIC "$<INSTALL_INTERFACE:include>")' portaudio/CMakeLists.txt
sed -i '307d' portaudio/CMakeLists.txt # SET(PA_LIBRARY_DEPENDENCIES ${PA_LIBRARY_DEPENDENCIES} ${ALSA_LIBRARIES})
sed -i '306d' portaudio/CMakeLists.txt # SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} -lasound")
sed -i '305d' portaudio/CMakeLists.txt

sed -i '305i  SET(PA_PRIVATE_COMPILE_DEFINITIONS ${PA_PRIVATE_COMPILE_DEFINITIONS} PA_USE_ALSA PA_ALSA_DYNAMIC)' portaudio/CMakeLists.txt
sed -i '305i  SET(PA_PKGCONFIG_LDFLAGS "${PA_PKGCONFIG_LDFLAGS} ${CMAKE_DL_LIBS}")' portaudio/CMakeLists.txt
      
      
cd portaudio/build

cmake .. \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_POSITION_INDEPENDENT_CODE=1 \
 -DPA_BUILD_SHARED=Off \
 -DPA_USE_JACK=Off \
 -DCMAKE_INSTALL_PREFIX=/usr

make -j$NPROC
make install
cd /
rm -rf /portaudio

git clone --recursive -j16 https://github.com/ossia/score
mkdir build
cd build
cmake -GNinja -Wno-dev ../score \
    -DCMAKE_UNITY_BUILD=1 \
    -DSCORE_CONFIGURATION=static-release \
    -DDEPLOYMENT_BUILD=1 \
    -DCMAKE_SKIP_RPATH=ON \
    -DCMAKE_INSTALL_PREFIX="/usr" 

cmake --build .
cpack -G DEB 
