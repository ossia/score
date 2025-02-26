#!/bin/bash
rm -rf release
mkdir -p release

cp -rf MultiCursor *.{hpp,cpp,txt,json} LICENSE release/

mv release score-addon-multicursor
7z a score-addon-multicursor.zip score-addon-multicursor
