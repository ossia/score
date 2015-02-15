#!/bin/bash -xe
cd ..
cppcheck -v --library=qt --xml --enable=all --quiet --std=c++11 --suppress=purgedConfiguration -i base/plugins/device_explorer -I plugins/scenario -I lib -I lib/core -I /home/jm/Qt/5.3/gcc_64/include/ base 2> cppcheck.xml
tools/cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-html
rm cppcheck.xml
nohup x-www-browser cppcheck-html/index.html&


