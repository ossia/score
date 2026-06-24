#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"

export OSSIA_SDK="/c/ossia-sdk-x86_64"
export DST="$SDK_DIR"
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"

mkdir -p "$INCLUDE"
mkdir -p "$LIB"
mkdir -p "$LIB/cmake/score"

export SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $SCRIPTDIR/create-sdk-common.sh

# Copy OS API headers
cp -rf "$OSSIA_SDK/llvm/include/." "$INCLUDE/"

# Copy our compiler's intrinsincs
export LLVM_VER=$(ls $OSSIA_SDK/llvm-libs/lib/clang/)
mkdir -p "$DST/usr/lib/clang/$LLVM_VER/include"

cp -rf "$OSSIA_SDK/llvm-libs/lib/clang/$LLVM_VER/include" "$LIB/clang/$LLVM_VER/"

# Ship the ORC runtime (orc_rt) so the JIT can use ExecutorNativePlatform.
ship_orc_runtime "$OSSIA_SDK/llvm-libs/lib/clang" "$LIB"

source $SCRIPTDIR/cleanup-sdk-common.sh

