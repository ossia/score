#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"

export OSSIA_SDK="/c/ossia-sdk"
export DST="$SDK_DIR"
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"

mkdir -p "$INCLUDE"
mkdir -p "$LIB"

export SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $SCRIPTDIR/create-sdk-common.sh

# Copy OS API headers
cp -rf "$OSSIA_SDK/llvm/include/"* "$INCLUDE/"

# Copy our compiler's intrinsincs
export LLVM_VER=$(ls $OSSIA_SDK/llvm-libs/lib/clang/)
mkdir -p "$DST/usr/lib/clang/$LLVM_VER/include"
rsync -ar "$OSSIA_SDK/llvm-libs/lib/clang/$LLVM_VER/include" "$LIB/clang/$LLVM_VER/"


