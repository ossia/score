#!/bin/bash

mkdir -p ~/travis-cache
cd ~/travis-cache
if [ ! -f homebrew-cache.tar.gz ]; then
  tar czf homebrew-cache.tar.gz --directory /usr/local/Cellar wget cmake qt5 ninja boost
fi
