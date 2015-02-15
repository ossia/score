#!/bin/bash -xe
cd ..
cppcheck -v --library=qt --xml --enable=all --quiet --std=c++11 --suppress=purgedConfiguration -i base/plugins/device_explorer -I plugins/scenario -I plugins/scenario/source -I plugins/curve_plugin -I plugins/inspector -I plugins/iscore_cohesion -I lib -I lib/core -I /usr/include/qt -I /usr/include/QtCore -I /usr/include/QtWidgets base 2> cppcheck.xml
tools/cppcheck-htmlreport --file=cppcheck.xml --report-dir=cppcheck-html
rm cppcheck.xml
nohup x-www-browser cppcheck-html/index.html&


