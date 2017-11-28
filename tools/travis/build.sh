#!/bin/bash -eux
set -o pipefail
export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '
# Note : to make the tests work under travis, they have to be changed in order not to require QApplication but only QCoreApplication
#    - LD_LIBRARY_PATH=/usr/lib64 make ExperimentalTest


case "$TRAVIS_OS_NAME" in
  linux)
  export CMAKE_BIN=$(readlink -f "$(find cmake/bin -name cmake -type f )")
  ;;
  osx)
  export PATH=/tmp/CMake.app/Contents/bin:$PATH
  export CMAKE_BIN=$(find /tmp/cmake-* -regex .*bin/cmake)
  ;;
esac
export CTEST_OUTPUT_ON_FAILURE=1

mkdir -p build
cd build

export CONFIG_FOLDER=`realpath ${0%/*}/configs`
source "$CONFIG_FOLDER/$CONF.sh"
