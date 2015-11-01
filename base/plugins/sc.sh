#!/bin/zsh

sed -i 's/#include "Process\/\(.*\)"/#include <Process\/\1>/' **/*.{hpp,cpp}
