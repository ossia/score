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

# Copy OS API headers
for package in glibc-headers glibc-devel alsa-lib-devel mesa-libGL-devel libxkbcommon-x11-devel kernel-headers ; do
  (
    cd /usr/include
    rsync -aR $(rpm -ql $package | grep '/usr/include' | grep '\.h' | cut -c 14-  | sort | uniq) "$INCLUDE/"
  )
done

# Copy clang lib headers
export LLVM_VER=$(ls $OSSIA_SDK/llvm/lib/clang/)
mkdir -p "$LIB/clang/$LLVM_VER/include"
rsync -ar "$OSSIA_SDK/llvm/lib/clang/$LLVM_VER/include/" "$LIB/clang/$LLVM_VER/include/"

