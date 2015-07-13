#!/bin/bash
# Note : to make the tests work under travis, they have to be changed in order not to require QApplication but only QCoreApplication
#    - LD_LIBRARY_PATH=/usr/lib64 make ExperimentalTest


mkdir build
cd build
export CMAKE_COMMON_FLAGS="-DCMAKE_BUILD_TYPE=Release -DISCORE_STATIC_PLUGINS:Bool=True -DDEPLOYMENT_BUILD:Bool=True"
case "$TRAVIS_OS_NAME" in
    linux)
        source /opt/qt54/bin/qt54-env.sh
		/usr/local/bin/cmake -DISCORE_COTIRE:Bool=True $CMAKE_COMMON_FLAGS ..

#		make all_unity -j2
		make package -j2
	;;
	osx)
		cmake -DCMAKE_PREFIX_PATH="/usr/local/Cellar/qt5/5.4.2/lib/cmake;$(pwd)/../Jamoma/share/cmake" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle $CMAKE_COMMON_FLAGS ..

#		make all_unity -j2
		make install -j2
	;;
esac
