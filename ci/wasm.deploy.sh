#!/bin/bash -eux
export SCORE_DIR=$PWD

mkdir site
cd site
git init

git config --local user.email "41898282+github-actions[bot]@users.noreply.github.com"
git config --local user.name "github-actions[bot]"

mv /build/*.js .
mv /build/ossia-score.wasm .
# Qt bundles preloaded plugins/resources into a .data file when present.
[ -f /build/ossia-score.data ] && mv /build/ossia-score.data .
mv $SCORE_DIR/cmake/Deployment/WASM/* .

git add .
git commit -m "continuous integration"
git branch -M main
git remote add origin git@github.com:ossia/score-web.git
