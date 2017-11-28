#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" -DBOOST_ROOT="$BOOST_ROOT" -DSCORE_CONFIGURATION=linux-gcov $CMAKE_COMMON_FLAGS ..
$CMAKE_BIN --build . --target all_unity -- -j2

echo "$PWD"
gem install coveralls-lcov
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 3
cp -rf ../Tests/testdata .
find . -name "*.o" -print0 | xargs -0 rm -rf
LD_LIBRARY_PATH=/usr/lib64 ./score_testapp
# LD_LIBRARY_PATH=/usr/lib64 $CMAKE_BIN --build . --target score_test_coverage_unity
lcov --compat-libtool --directory .. --capture --output-file coverage.info
lcov --remove coverage.info '*.moc' '*/moc_*' '*/qrc_*' '*/ui_*' '*/tests/*' '/usr/*' '/opt/*' '*/3rdparty/*' --output-file coverage.info.cleaned
mv coverage.info.cleaned coverage.info
coveralls-lcov coverage.info
