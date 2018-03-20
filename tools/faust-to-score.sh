#!/bin/bash -eux

if [[ "$1" == "" ]]; 
	then
	exit 1
fi

FAUST_SRC=$1
FAUST_ARCH=/home/jcelerier/i-score/base/plugins/score-plugin-media/Media/Effect/Faust/faust-score-arch.cpp
faust -a "$FAUST_ARCH" "$FAUST_SRC" -o /tmp/__score_faust_source.cpp

FAUST_AUTHOR=$(grep 'author: ' /tmp/__score_faust_source.cpp | sed 's/author: "//' | sed 's/"//' | awk '{print $1}')
FAUST_NAME=$(grep 'name: ' /tmp/__score_faust_source.cpp | sed 's/name: "//' | sed 's/"//' | awk '{print $1}')
UUID=$(uuidgen)
sed -i "s/==FAUST_NAME==/$FAUST_NAME/g" /tmp/__score_faust_source.cpp
sed -i "s/==UUID==/$UUID/g" /tmp/__score_faust_source.cpp

rm -rf "score-faust-$FAUST_NAME"
mkdir "score-faust-$FAUST_NAME"
cd "score-faust-$FAUST_NAME"

echo "
cmake_minimum_required(VERSION 3.1)
set(CMAKE_AUTOMOC ON)
project(score_faust_$FAUST_NAME LANGUAGES CXX)

score_common_setup()

add_library(score_faust_$FAUST_NAME score_faust_$FAUST_NAME.cpp)

target_link_libraries(score_faust_$FAUST_NAME PUBLIC score_plugin_engine score_plugin_media)

setup_score_plugin(score_faust_$FAUST_NAME)

" >  CMakeLists.txt

mv /tmp/__score_faust_source.cpp "score_faust_$FAUST_NAME.cpp"
echo "#include \"score_faust_$FAUST_NAME.moc\"" >> "score_faust_$FAUST_NAME.cpp"
