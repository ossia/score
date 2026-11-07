#!/bin/bash
# Prints the newest LLVM major version an apt.llvm.org repository has a release
# branch of, e.g. 23 for https://apt.llvm.org/resolute, whose dists/ lists
# llvm-toolchain-resolute-21, -22, -23 and the unversioned snapshot of main.
set -euo pipefail

url=${1:?usage: $0 https://apt.llvm.org/<codename>}
codename=${url%/}
codename=${codename##*/}

version=$(curl -fsSL --compressed --retry 3 "${url%/}/dists/" \
  | { grep -oE "llvm-toolchain-${codename}-[0-9]+" || true; } \
  | sed -E 's/.*-([0-9]+)$/\1/' \
  | sort -n | tail -n 1)

if [ -z "$version" ]; then
  echo "no llvm-toolchain-${codename}-<version> in ${url%/}/dists/" >&2
  exit 1
fi
echo "$version"
