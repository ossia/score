#!/bin/bash
export PATH=/usr/local/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/lib
export NPROC=$(nproc)

export CC="arm-linux-gnueabihf-gcc-9"
export CXX="arm-linux-gnueabihf-g++-9"
export CPP="arm-linux-gnueabihf-cpp-9"
export AR="arm-linux-gnueabihf-gcc-ar-9"
export RANLIB="arm-linux-gnueabihf-gcc-ranlib-9"
export LD="$CXX"

export CFLAGS="-O3 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -g0"
export CXXFLAGS="-O3 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -g0"

(
mkdir -p image
cd /image
git clone --recursive -j16 https://github.com/OSSIA/score
)

(
mkdir -p /image/build
cd /image/build

cmake ../score \
	-DSCORE_CONFIGURATION=static-release \
	-DCMAKE_INSTALL_PREFIX=/opt/score \
	-DCMAKE_BUILD_UNITY=1 \
	-DDEPLOYMENT_BUILD=1 \
	-DCMAKE_PREFIX_PATH=/usr/local/Qt-5.13.0 \
	-DCMAKE_AR=/usr/local/bin/arm-linux-gnueabihf-gcc-ar-9 \
	-DCMAKE_RANLIB=/usr/local/bin/arm-linux-gnueabihf-gcc-ranlib-9 \
	-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=gold" \
	-DCMAKE_SYSTEM_PROCESSOR=arm

make -j8
make install
)

(
cd /image/score/cmake/Deployment/Linux/Raspberry/
cp ossia-score.sh /opt/score/bin/
cp qt.conf /opt/score/bin/
cp -rf /usr/local/Qt-5.13.0/lib/* /opt/score/lib/
cp -rf /usr/local/Qt-5.13.0/plugins /opt/score/lib/
cp -rf /usr/local/Qt-5.13.0/qml /opt/score/lib/
cp -rf /usr/local/lib/libgcc_s.so.1 /opt/score/lib/
cp -rf /usr/local/lib/libstdc++.so.6 /opt/score/lib/
)

(
cd /opt
tar caf score.tar.gz score
)