#!/bin/bash

cd ~/travis-cache
if [ -f homebrew-cache.tar.gz ]; then
  if ! tar tf homebrew-cache.tar.gz &>/dev/null; then
    rm homebrew-cache.tar.gz
    exit 0
  fi
  tar zxf homebrew-cache.tar.gz --directory /usr/local/Cellar
  brew link boost cmake ninja qt5 wget
fi
