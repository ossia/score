#!/bin/bash -eux

: "$SDK_DIR"
: "$SCORE_DIR"

if [[ "${RUNNER_ARCH:-X64}" == "ARM64" ]]; then
  export OSSIA_SDK="/c/ossia-sdk-aarch64"
else
  export OSSIA_SDK="/c/ossia-sdk-x86_64"
fi
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

# Ship compiler-rt's builtins archive too. The JIT compiles add-ons with bare -cc1
# (no driver), so the runtime helpers the driver would pull via -rtlib=compiler-rt
# (e.g. __udivti3, used by fmt's 128-bit formatting) must be resolvable at JIT load
# time. locateBuiltinsRuntime() finds it next to orc_rt (same clang/<ver>/lib/<t>/).
for CLANGROOT in "$OSSIA_SDK/llvm-libs/lib/clang" "$OSSIA_SDK/llvm/lib/clang"; do
  [[ -d "$CLANGROOT" ]] || continue
  while IFS= read -r f; do
    rel="${f#"$CLANGROOT"/}"
    mkdir -p "$LIB/clang/$(dirname "$rel")"
    cp -f "$f" "$LIB/clang/$rel"
    echo "shipped builtins: clang/$rel"
  done < <(find "$CLANGROOT" -name 'libclang_rt.builtins*.a')
done

# Ship mingw-w64's CRT-support archives too (the driver's implicit -lmingwex /
# -lucrt): libmingwex has the C99-name math functions (hypotf, ...) that the UCRT
# only exports under their underscore names; libucrt's alias objects have the
# POSIX old names (fileno) and renamed imports (__msvcrt_assert).
# locateMingwRuntimeLib() finds them at <sdk>/lib/.
for CRTLIB in libmingwex.a libucrt.a; do
  SRC="$OSSIA_SDK/llvm/x86_64-w64-mingw32/lib/$CRTLIB"
  [[ -f "$SRC" ]] || continue
  cp -f "$SRC" "$LIB/$CRTLIB"
  echo "shipped crt-support: lib/$CRTLIB"
done

source $SCRIPTDIR/cleanup-sdk-common.sh

