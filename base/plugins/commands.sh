#!/bin/bash
grep -R 'ISCORE_COMMAND_DECL(' | cut -d',' -f2 | sed -e 's/^[ \t]*//'

