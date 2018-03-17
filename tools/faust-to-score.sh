#!/bin/bash

FAUST_SRC=/home/jcelerier/travail/faust/examples/old/freeverb.dsp
FAUST_ARCH=/home/jcelerier/i-score/base/plugins/score-plugin-media/Media/Effect/Faust/faust-score-arch.cpp
faust -a "$FAUST_ARCH" "$FAUST_SRC" -o /tmp/__score_faust_source.cpp

FAUST_AUTHOR=$(grep 'author: ' /tmp/__score_faust_source.cpp | sed 's/author: "//' | sed 's/"//' | awk '{print $1}')
FAUST_NAME=$(grep 'name: ' /tmp/__score_faust_source.cpp | sed 's/name: "//' | sed 's/"//' | awk '{print $1}')
UUID=$(uuidgen)
sed -i "s/==FAUST_NAME==/$FAUST_NAME/g" /tmp/__score_faust_source.cpp
sed -i "s/==UUID==/$UUID/g" /tmp/__score_faust_source.cpp
cat  /tmp/__score_faust_source.cpp
