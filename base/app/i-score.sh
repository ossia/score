#!/bin/sh
if [ -f ./i-score ]; then
    LD_LIBRARY_PATH=.:/usr/lib/i-score ./i-score
else
    LD_LIBRARY_PATH=lib:lib/i-score bin/i-score
fi
