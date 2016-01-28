#!/bin/sh
source "$CONFIG_FOLDER/linux-source-qt.sh"

$CMAKE_BIN -DISCORE_CONFIGURATION=linux-gcov ..
$CMAKE_BIN --build . --target all_unity -- -j2

echo "$PWD"
gem install coveralls-lcov
export DISPLAY=:99.0
sh -e /etc/init.d/xvfb start
sleep 3
cp -rf ../Tests/testdata .
$CMAKE_BIN --build . --target iscore_test_coverage
lcov --compat-libtool --directory .. --capture --output-file coverage.info --no-external
coveralls-lcov --repo-token jjoMcOyOg9R05XT3aVysqTcsL1gyAc9tF coverage.info
