#!/bin/bash
grep -R 'SCORE_COMMAND_DECL(' | cut -d',' -f2 | sed -e 's/^[ \t]*//'

grep -R 'SCORE_COMMAND_DECL_T(' | cut -d'(' -f2 | sed -e 's/^[ \t]*//;s/.$//'
