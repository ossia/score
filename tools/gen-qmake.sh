#!/bin/zsh

setopt null_glob
(
cd ..
HEADERS=$(ls base/{plugins,lib}/**/*.{h,hpp} | egrep -v "(test|Test)")
SOURCES=$(ls base/{plugins,lib}/**/*.cpp | egrep -v "(test|Test|main.cpp)")
RESOURCES=$(ls base/{plugins,lib}/**/*.qrc)

HEADERS=$(echo "$HEADERS" |sed 's/^/HEADERS+=/g')
SOURCES=$(echo "$SOURCES" |sed 's/^/SOURCES+=/g')
RESOURCES=$(echo "$RESOURCES" |sed 's/^/RESOURCES+=/g')
echo "
$HEADERS

$SOURCES

$RESOURCES
" > score-srcs.pri
)

(
cd ../API

HEADERS=$(ls OSSIA/**/*.{h,hpp})
SOURCES=$(ls OSSIA/**/*.cpp)

HEADERS=$(echo "$HEADERS" |sed 's/^/HEADERS+=API\//g')
SOURCES=$(echo "$SOURCES" |sed 's/^/SOURCES+=API\//g')
echo "
$HEADERS

$SOURCES

" >> ../score-srcs.pri
)

(
cd ..
mkdir build
cd build
(
mkdir cmake-tmp
cd cmake-tmp
cmake -DSCORE_CONFIGURATION=static-release ../../
cp **/*.{h,hpp,cpp} ../
)
rm -rf cmake-tmp
# qmake ..
)

