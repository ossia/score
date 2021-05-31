#!/bin/bash

echo "Running on OSTYPE: '$OSTYPE'"

export LATEST_TAG=$(git describe --tags --abbrev=0)
export LATEST_TAG_NOV=$(echo "$LATEST_TAG" | sed "s/v//")
export BASE_SDK=https://github.com/ossia/score-sdk/releases/download/sdk21
export LATEST_RELEASE=https://github.com/ossia/score/releases/download/$LATEST_TAG

if [[ "$OSTYPE" == "darwin"* ]]; then
(
  # First download the compiler and base libraries
  sudo mkdir -p /opt/ossia-sdk-x86_64/
  sudo chown -R $(whoami) /opt
  sudo chmod -R a+rwx /opt

  SDK_ARCHIVE=sdk-macOS.tar.gz
  wget -nv "$BASE_SDK/$SDK_ARCHIVE" -O "$SDK_ARCHIVE"
  tar -xzf "$SDK_ARCHIVE" --strip-components=2 --directory /opt/ossia-sdk-x86_64

  ls /opt/ossia-sdk-x86_64/

  # Then the score headers
  SCORE_SDK_ARCHIVE=mac-sdk.zip
  wget -nv "$LATEST_RELEASE/$SCORE_SDK_ARCHIVE" -O "$SCORE_SDK_ARCHIVE"
  mkdir -p "$HOME/Documents/ossia score library/SDK/"
  unzip -qq $SCORE_SDK_ARCHIVE -d "$HOME/Documents/ossia score library/SDK/$LATEST_TAG_NOV"
)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
(
  # First download the compiler and base libraries
  sudo mkdir -p /opt/ossia-sdk
  sudo chown -R $(whoami) /opt/ossia-sdk

  wget -nv $BASE_SDK/sdk-linux.tar.xz
  tar xaf sdk-linux.tar.xz --strip-components=2 --directory /opt/ossia-sdk
  rm -rf sdk-linux.tar.xz

  ls /opt/ossia-sdk/

  # Then the score headers
  SCORE_SDK_ARCHIVE=linux-sdk.zip
  wget -nv "$LATEST_RELEASE/$SCORE_SDK_ARCHIVE" -O "$SCORE_SDK_ARCHIVE"
  mkdir -p "$HOME/Documents/ossia score library/SDK/"
  unzip -qq $SCORE_SDK_ARCHIVE -d "$HOME/Documents/ossia score library/SDK/$LATEST_TAG_NOV"
)
else
(
  # First download the compiler and base libraries
  mkdir /c/ossia-sdk
  cd /c/ossia-sdk

  curl -L -O $BASE_SDK/sdk-mingw.7z
  7z x sdk-mingw.7z
  rm sdk-mingw.7z

  ls /c/ossia-sdk/

  # Then the score headers
  SCORE_SDK_ARCHIVE=windows-sdk.zip
  wget -nv "$LATEST_RELEASE/$SCORE_SDK_ARCHIVE" -O "$SCORE_SDK_ARCHIVE"
  mkdir -p "$HOME/Documents/ossia score library/SDK/"
  7z x $SCORE_SDK_ARCHIVE  -o"$HOME/Documents/ossia score library/SDK/$LATEST_TAG_NOV"
)
fi
