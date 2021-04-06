#!/bin/bash

if [[ "$OSTYPE" == "darwin"* ]]; then
(
  SDK_ARCHIVE=sdk-macOS.tar.gz
  wget -nv https://github.com/ossia/score-sdk/releases/download/sdk20/$SDK_ARCHIVE -O $SDK_ARCHIVE
  sudo mkdir -p /opt/ossia-sdk-x86_64/
  sudo chown -R $(whoami) /opt
  sudo chmod -R a+rwx /opt
  gtar xhaf $SDK_ARCHIVE --strip-components=2 --directory /opt/ossia-sdk-x86_64/
  ls /opt/ossia-sdk-x86_64/
)
elif [[ "$OSTYPE" == "linux*" ]]; then
(
  wget -nv https://github.com/ossia/sdk/releases/download/sdk19/sdk-linux.tar.xz
  tar xaf sdk-linux.tar.xz --strip-components=2 --directory /opt/ossia-sdk
  rm -rf sdk-linux.tar.xz
  sudo mkdir -p /opt/ossia-sdk 
  sudo chown -R $(whoami) /opt/ossia-sdk
)
else
(
  mkdir /c/ossia-sdk
  cd /c/ossia-sdk
  curl -L https://github.com/ossia/sdk/releases/download/sdk19/sdk-mingw.7z --output sdk-mingw.7z
  7z x sdk-mingw.7z
  rm sdk-mingw.7z
)
fi
