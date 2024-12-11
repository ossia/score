#!/bin/bash

if [[ -f /etc/redhat-release ]]; then
  # Fedora does not ship this for some obscure reason
  if [[ ! -f "$DIR/lib/libbz2.so.1.0" ]]; then
    find /usr -name libbz2.so.1 -exec cp {} "$DIR/lib/libbz2.so.1.0" \; || true
  fi

  # Fedora has a recent enough libstdc++
  rm "$DIR/lib/libstdc++.so.6"
fi

if [[ -f /etc/arch-release ]]; then
  # Arch has a recent enough libstdc++
  rm "$DIR/lib/libstdc++.so.6"
fi