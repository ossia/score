#!/bin/bash -eux
export SCORE_DIR=$PWD

mkdir site
cd site

mv /build/ossia-score.js .
mv /build/ossia-score.wasm .
mv $SCORE_DIR/cmake/Deployment/WASM/* .
echo 'web.ossia.io' > CNAME
