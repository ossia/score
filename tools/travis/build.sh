#!/bin/bash -eux
set -o pipefail
export PS4='+ ${FUNCNAME[0]:+${FUNCNAME[0]}():}line ${LINENO}: '
# Note : to make the tests work under travis, they have to be changed in order not to require QApplication but only QCoreApplication
#    - LD_LIBRARY_PATH=/usr/lib64 make ExperimentalTest


mkdir build
cd build
export CMAKE_COMMON_FLAGS="-GNinja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DISCORE_STATIC_PLUGINS:Bool=$STATIC_PLUGINS -DDEPLOYMENT_BUILD:Bool=$DEPLOYMENT -DISCORE_COTIRE:Bool=True -DISCORE_COVERAGE:Bool=$USE_GCOV"
export CTEST_OUTPUT_ON_FAILURE=1

if [[ "$USE_COVERITY" = "False" ]];
then
  case "$TRAVIS_OS_NAME" in

    linux)
      if [[ "$STATIC_QT" = "True" ]];
      then
        /usr/local/bin/cmake $CMAKE_COMMON_FLAGS -DISCORE_STATIC_QT:Bool=True -DCMAKE_PREFIX_PATH="/usr/local/jamoma/share/cmake/Jamoma;/opt/qt5-static/lib/cmake/Qt5"  ..
        cmake --build . --target package
      else
        set +eux; source /opt/qt55/bin/qt55-env.sh; set -eux
        /usr/local/bin/cmake $CMAKE_COMMON_FLAGS ..

        if [[ "$DEPLOYMENT" = "True" ]];
        then
          cmake --build . --target package --config DynamicRelease
        else
          ninja
        fi

        if [[ "$USE_GCOV" = "True" ]];
        then
          echo "$PWD"
          gem install coveralls-lcov
          export DISPLAY=:99.0
          sh -e /etc/init.d/xvfb start
          sleep 3
          cp -rf ../Tests/testdata .
          ninja iscore_test_coverage
          lcov --compat-libtool --directory .. --capture --output-file coverage.info --no-external
          coveralls-lcov --repo-token jjoMcOyOg9R05XT3aVysqTcsL1gyAc9tF coverage.info
        fi
      fi
    ;;

    osx)
      export QT_PATH=$(dirname $(dirname $(find /usr/local/Cellar/qt5 -name Qt5Config.cmake) ) )
      cmake -DCMAKE_PREFIX_PATH="$QT_PATH;$(pwd)/../Jamoma/share/cmake" -DCMAKE_INSTALL_PREFIX=$(pwd)/bundle $CMAKE_COMMON_FLAGS ..

      cmake --build . --target install/strip --config DynamicRelease
    ;;
  esac
else
  if [[ "$TRAVIS_BRANCH" = "$COVERITY_SCAN_BRANCH_PATTERN" ]];
  then
    set +eux; source /opt/qt55/bin/qt55-env.sh; set -eux
    /usr/local/bin/cmake -DISCORE_COTIRE:Bool=False $CMAKE_COMMON_FLAGS ..

    eval "$COVERITY_SCAN_BUILD"
  fi
fi
