#!/bin/bash -eux
## CMake
CMAKE_EXE=cmake-3.12.3-Linux-x86_64.sh

wget https://cmake.org/files/v3.12/$CMAKE_EXE
chmod +x $CMAKE_EXE
mkdir /cmake
./$CMAKE_EXE --prefix=/cmake --skip-license

(
cd /image
mkdir build
cd build

mkdir /llvm-bootstrap
/cmake/bin/cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_INCLUDE_TOOLS=1 -DLLVM_BUILD_TOOLS=1 -DBUILD_SHARED_LIBS=0  -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_INCLUDE_EXAMPLES=0 -DLLVM_INCLUDE_TESTS=0 -DLLVM_ENABLE_CXX1Y=1 -DCMAKE_INSTALL_PREFIX=/llvm-bootstrap ../llvm
/cmake/bin/cmake --build . -- -j6
/cmake/bin/cmake --build . --target install

cd ..
rm -rf build
)

(
cd /image
mkdir build
cd build

export LD_LIBRARY_PATH=/llvm-bootstrap/lib
/cmake/bin/cmake \
 -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_C_COMPILER=/llvm-bootstrap/bin/clang \
 -DCMAKE_CXX_COMPILER=/llvm-bootstrap/bin/clang++ \
 -DLLVM_INCLUDE_TOOLS=1 \
 -DLLVM_BUILD_TOOLS=1 \
 -DBUILD_SHARED_LIBS=0 \
 -DLLVM_TARGETS_TO_BUILD="X86" \
 -DLLVM_INCLUDE_EXAMPLES=0 \
 -DLLVM_INCLUDE_TESTS=0 \
 -DLLVM_ENABLE_CXX1Y=1 \
 -DLLVM_ENABLE_LIBCXX=On \
 -DLLVM_ENABLE_LIBCXXABI=On \
 -DCMAKE_INSTALL_PREFIX=/usr/local \
  ../llvm
 
/cmake/bin/cmake --build . -- -j6
/cmake/bin/cmake --build . --target install

cd ..
rm -rf build
)