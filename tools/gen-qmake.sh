#!/bin/zsh

(
cd ..

HEADERS=$(ls base/{plugins,lib}/**/*.hpp | egrep -v "test")
SOURCES=$(ls base/{plugins,lib}/**/*.cpp | egrep -v "(test|main.cpp)")
RESOURCES=$(ls base/{plugins,lib}/**/*.qrc)

HEADERS=$(echo "$HEADERS" |sed 's/^/HEADERS+=/g')
SOURCES=$(echo "$SOURCES" |sed 's/^/SOURCES+=/g')
RESOURCES=$(echo "$RESOURCES" |sed 's/^/RESOURCES+=/g')
echo "
$HEADERS

$SOURCES

$RESOURCES
" > i-score-srcs.pri
)
