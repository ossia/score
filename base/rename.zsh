#!/bin/zsh

perl-rename 's/$1/$2/g' **/*
sed -i 's/$1/$2/g' **/*.{hpp,cpp,txt}
