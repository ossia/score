#!/bin/sh
grep -R 'class '|egrep -v 'class .*;'|cut -d':' -f1|grep -o '[^/]*$'|cut -d'.' -f1|sort > paths
grep -R 'class '|egrep -v 'class .*;'|cut -d':' -f2|cut -d' ' -f2|sort > classnames

comm -3 paths classnames
rm paths
rm classnames
