#!/bin/bash
# Note : to make the tests work under travis, they have to be changed in order not to require QApplication but only QCoreApplication
#    - LD_LIBRARY_PATH=/usr/lib64 make ExperimentalTest


mkdir build
cd build
export CMAKE_COMMON_FLAGS="-GNinja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DISCORE_STATIC_PLUGINS:Bool=$STATIC_PLUGINS -DDEPLOYMENT_BUILD:Bool=$DEPLOYMENT -DISCORE_COTIRE:Bool=True"
export CTEST_OUTPUT_ON_FAILURE=1

if [[ "$USE_COVERITY" = "False" ]];
then
  case "$TRAVIS_OS_NAME" in
    linux)
      source /opt/qt55/bin/qt55-env.sh
      /usr/local/bin/cmake $CMAKE_COMMON_FLAGS ..

      if [[ "$DEPLOYMENT_BUILD" = "True" ]];
      then
        ninja package -j2
      else
        ninja -j2
      fi
    ;;
    osx)
      cmake -DCMAKE_PREFIX_PATH="/usr/local/Cellar/qt5/5.5.1_1/lib/cmake;$(pwd)/../Jamoma/share/cmake" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle $CMAKE_COMMON_FLAGS ..

      ninja install/strip -j2
    ;;
  esac
else
  if [[ "$TRAVIS_BRANCH" = "$COVERITY_SCAN_BRANCH_PATTERN" ]];
  then
    source /opt/qt55/bin/qt55-env.sh
    /usr/local/bin/cmake -DISCORE_COTIRE:Bool=False $CMAKE_COMMON_FLAGS ..

    eval "$COVERITY_SCAN_BUILD"
  fi
fi
