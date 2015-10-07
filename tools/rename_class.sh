#!/bin/zsh -eux

SOURCENAME=$1
DESTNAME=$2

sed -i "s/$SOURCENAME/$DESTNAME/g" **/*.{hpp,cpp,txt}
HPP_FILE=$(find . -name "$SOURCENAME.hpp")
CPP_FILE=$(find . -name "$SOURCENAME.cpp")

if [[ $HPP_FILE != "" ]];
then
  HPP_DIR=$(dirname $HPP_FILE)
  mv "$HPP_FILE" "$HPP_DIR/$DESTNAME.hpp"
fi

if [[ $CPP_FILE != "" ]];
then
  CPP_DIR=$(dirname $CPP_FILE)
  mv "$CPP_FILE" "$CPP_DIR/$DESTNAME.cpp"
fi
