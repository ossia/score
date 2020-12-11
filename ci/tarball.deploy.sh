#!/bin/bash -eux
if [[ "$GITTAG" = "" ]]; then
    GITTAG=devel
fi

export TAG=$(echo $GITTAG | tr -d v)

pwd
ls
find / -name "ossia-score.tar.xz"
mkdir deploy
mv "ossia-score.tar.xz" "deploy/ossia score-$TAG-src.tar.xz"
mv "ossia-score.tar.xz.asc" "deploy/ossia score-$TAG-src.tar.xz.asc"
