#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"

export OSSIA_SDK="/opt/ossia-sdk"
export DST="$SDK_DIR"
export INCLUDE="$DST/usr/include"
export LIB="$DST/usr/lib"

mkdir -p "$INCLUDE"
mkdir -p "$LIB"

export SCRIPTDIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
source $SCRIPTDIR/create-sdk-common.sh

export XCODE=/Applications/Xcode.app/Contents/Developer
export XCODE_MACOS_SDK=$XCODE/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
export XCODE_TOOLCHAIN=$XCODE/Toolchains/XcodeDefault.xctoolchain

# Copy OS API headers
rsync -ar "$XCODE_MACOS_SDK/usr/include/" "$INCLUDE/"
rsync -ar "$XCODE_TOOLCHAIN/usr/include/c++" "$INCLUDE/"

# Copy clang lib headers
export LLVM_VER=$(ls $XCODE_TOOLCHAIN/usr/lib/clang/)
mkdir -p "$LIB/clang/$LLVM_VER"
rsync -ar "$XCODE_TOOLCHAIN/usr/lib/clang/$LLVM_VER/include" "$LIB/clang/$LLVM_VER/"

