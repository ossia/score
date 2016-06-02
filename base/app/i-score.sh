#!/bin/sh
if [ -f ./i-score ]; then
    LD_LIBRARY_PATH=.:/usr/lib/i-score ./i-score
else
    export QT_XKB_CONFIG_ROOT=/usr/share/X11/xkb
    LD_LIBRARY_PATH=lib:lib/i-score bin/i-score
fi
