#!/bin/bash

for f in *Variable*ttf; do
  for i in {100..900..100}; do
    fonttools varLib.instancer $f wght=$i -o $f.$i.ttf
    
  done
done
