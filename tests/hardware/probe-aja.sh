#!/bin/sh
# Hardware probe: is an AJA NTV2 card present with its driver loaded?
# The ajantv2 driver creates one character device per board.
# Exit 0 = present, non-zero = absent.
for d in /dev/ajantv2*; do
  [ -c "$d" ] && exit 0
done
exit 1
