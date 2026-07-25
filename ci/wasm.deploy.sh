#!/bin/bash -eux
export SCORE_DIR=$PWD

mkdir site
cd site
git init

git config --local user.email "41898282+github-actions[bot]@users.noreply.github.com"
git config --local user.name "github-actions[bot]"

mv /build/*.js .
mv /build/ossia-score.wasm .
# Preloaded MEMFS image: Qt's bundled plugins/resources, plus the user library
# when it was baked in. Not an `&&` one-liner: under `set -e` a missing file
# would abort the deploy.
if [ -f /build/ossia-score.data ]; then
  mv /build/ossia-score.data .
fi
mv $SCORE_DIR/cmake/Deployment/WASM/* .

git add .
git commit -m "continuous integration"
git branch -M main
git remote add origin git@github.com:ossia/score-web.git
