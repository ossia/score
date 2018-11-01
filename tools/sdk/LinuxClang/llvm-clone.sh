#!/bin/bash -eux
## CMake
CMAKE_EXE=cmake-3.12.3-Linux-x86_64.sh
LLVM_VER="RELEASE_700"

wget https://cmake.org/files/v3.12/$CMAKE_EXE
chmod +x $CMAKE_EXE
mkdir /cmake
./$CMAKE_EXE --prefix=/cmake --skip-license

yum install -y svn perl-Data-Dump perl-Data-Dumper


cd /image
(
svn co http://llvm.org/svn/llvm-project/llvm/tags/$LLVM_VER/final llvm
cd llvm/tools
svn co http://llvm.org/svn/llvm-project/cfe/tags/$LLVM_VER/final clang
cd ../..

#~ cd llvm/tools/clang/tools
#~ svn co http://llvm.org/svn/llvm-project/clang-tools-extra/tags/$LLVM_VER/final extra
#~ cd ../../../..

cd llvm/tools
svn co http://llvm.org/svn/llvm-project/lld/tags/$LLVM_VER/final lld
cd ../..

cd llvm/tools
svn co http://llvm.org/svn/llvm-project/polly/tags/$LLVM_VER/final polly
cd ../..

#~ cd llvm/tools
#~ svn co http://llvm.org/svn/llvm-project/lldb/tags/$LLVM_VER/final lldb
#~ cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/$LLVM_VER/final compiler-rt
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/openmp/tags/$LLVM_VER/final openmp
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/libcxx/tags/$LLVM_VER/final libcxx
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/libcxxabi/tags/$LLVM_VER/final libcxxabi
cd ../..

mkdir /llvm-bootstrap
)

(
cd /image
mkdir build
cd build

/cmake/bin/cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_INCLUDE_TOOLS=1 -DLLVM_BUILD_TOOLS=1 -DBUILD_SHARED_LIBS=0  -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_INCLUDE_EXAMPLES=0 -DLLVM_INCLUDE_TESTS=0 -DLLVM_ENABLE_CXX1Y=1 -DCMAKE_INSTALL_PREFIX=/llvm-bootstrap ../llvm
/cmake/bin/cmake --build . -- -j$(nproc)
/cmake/bin/cmake --build . --target install

cd ..
rm -rf build
)

(
cd /image
mkdir build
cd build
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
 
/cmake/bin/cmake --build . -- -j$(nproc)
/cmake/bin/cmake --build . --target install

cd ..
rm -rf build
)