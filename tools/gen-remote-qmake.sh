#!/bin/zsh
OUTPUT_FILE=i-score-remote-srcs.pri
setopt null_glob
(
cd ..

HEADERS=$(ls base/lib/**/*.{h,hpp} | egrep -v "(test|Test)")
SOURCES=$(ls base/lib/**/*.cpp | egrep -v "(test|Test|/main.cpp)")

for folder in remote plugins/iscore-lib-state plugins/iscore-lib-device ; do
    SUB_H=$(ls base/$folder/**/*.{h,hpp} | egrep -v "(test|Test)")
    SUB_C=$(ls base/$folder/**/*.cpp | egrep -v "(test|Test|/main.cpp)")
    
    HEADERS="$HEADERS\n$SUB_H"
    SOURCES="$SOURCES\n$SUB_C"
done

HEADERS=$(echo "$HEADERS" |sed 's/^/HEADERS+=/g')
SOURCES=$(echo "$SOURCES" |sed 's/^/SOURCES+=/g')

echo "
$HEADERS

$SOURCES

" > $OUTPUT_FILE
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

" >> ../$OUTPUT_FILE
)
sed -i '/network\/serial/d' ../$OUTPUT_FILE
sed -i '/ossia\-pd/d' ../$OUTPUT_FILE
sed -i '/ossia\-java/d' ../$OUTPUT_FILE
sed -i '/midi/d' ../$OUTPUT_FILE
sed -i '/MIDI/d' ../$OUTPUT_FILE

exit 0
(
cd ..
mkdir -p build
cd build
(
mkdir -p cmake-tmp
cd cmake-tmp
cmake -DISCORE_CONFIGURATION=static-release ../../
cp **/*.{h,hpp,cpp} ../
)
rm -rf cmake-tmp
# qmake ..
)

