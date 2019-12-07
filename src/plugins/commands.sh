#!/bin/bash
grep -R 'SCORE_COMMAND_DECL(' | cut -d',' -f2 | sed -e 's/^[ \t]*//'

grep -R 'SCORE_COMMAND_DECL_T(' | cut -d'(' -f2 | sed -e 's/^[ \t]*//;s/.$//'

rg '#include <'   | cut -d ':' -f2 |grep -v '<score' | grep -v '<Q' | grep -v '<core'| grep -v '<ossia' | grep  -P --only-matching  '<([a-zA-Z]+)/' | sort | uniq

