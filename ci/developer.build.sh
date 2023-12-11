#!/bin/bash -eux

if [[ $# -lt 4 ]]; then
  echo "Pass paths to source and build dir as you would for any CMake script: -S '<source dir>' -B '<build dir>'"
  exit 1
fi

if [[ -x "$(command -v ld.mold)" ]]; then
  LINKER=" -fuse-ld=mold -Wl,-z,now "
elif [[ -x "$(command -v ld.lld)" ]]; then
  LINKER=" -fuse-ld=lld -Wl,-z,now "
else
  LINKER=""
fi

ARGS=(
  -GNinja 
  -DCMAKE_C_COMPILER=clang 
  -DCMAKE_CXX_COMPILER=clang++ 
  -DCMAKE_C_FLAGS="-Wall -Wextra -pedantic -Woverloaded-virtual -Werror=return-type" 
  -DCMAKE_CXX_FLAGS="-Wall -Wextra -pedantic -Woverloaded-virtual -Werror=return-type" 
  -DCMAKE_EXE_LINKER_FLAGS=" $LINKER " 
  -DCMAKE_SHARED_LINKER_FLAGS=" $LINKER " 
  -DCMAKE_MODULE_LINKER_FLAGS=" $LINKER " 
  -DCMAKE_OPTIMIZE_DEPENDENCIES=1 
  -DCMAKE_LINK_DEPENDS_NO_SHARED=1 
  -DSCORE_PCH=1 
  -DSCORE_DYNAMIC_PLUGINS=1 
  -DCMAKE_POSITION_INDEPENDENT_CODE=1 
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 
  -DCMAKE_BUILD_TYPE=Debug
)

cmake -S "$2" -B "$4" "${ARGS[@]}"
cmake --build "$4"
