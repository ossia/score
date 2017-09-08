#!/bin/sh
if [ -f ./score ]; then
    LD_LIBRARY_PATH=.:/usr/lib/score ./score
else
    export QT_XKB_CONFIG_ROOT=/usr/share/X11/xkb
    LD_LIBRARY_PATH=lib:lib/score bin/score
fi
