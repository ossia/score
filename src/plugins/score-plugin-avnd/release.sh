#!/bin/bash
rm -rf release
mkdir -p release

cp -rf SimpleApi2 *.{hpp,cpp,txt,json} LICENSE release/

mv release score-addon-simpleapi2
7z a score-addon-simpleapi2.zip score-addon-simpleapi2
