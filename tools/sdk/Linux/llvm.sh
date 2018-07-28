#!/bin/bash -eux
## CMake
wget https://cmake.org/files/v3.12/cmake-3.12.0-Linux-x86_64.sh
chmod +x cmake-3.12.0-Linux-x86_64.sh
mkdir /cmake
./cmake-3.12.0-Linux-x86_64.sh --prefix=/cmake --skip-license

yum install -y svn perl-Data-Dump perl-Data-Dumper

cd /image
svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_600/final llvm
cd llvm/tools
svn co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_600/final clang
cd ../..

#~ cd llvm/tools/clang/tools
#~ svn co http://llvm.org/svn/llvm-project/clang-tools-extra/tags/RELEASE_600/final extra
#~ cd ../../../..

#~ cd llvm/tools
#~ svn co http://llvm.org/svn/llvm-project/lld/tags/RELEASE_600/final lld
#~ cd ../..

cd llvm/tools
svn co http://llvm.org/svn/llvm-project/polly/tags/RELEASE_600/final polly
cd ../..

#~ cd llvm/tools
#~ svn co http://llvm.org/svn/llvm-project/lldb/tags/RELEASE_600/final lldb
#~ cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_600/final compiler-rt
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/openmp/tags/RELEASE_600/final openmp
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/libcxx/tags/RELEASE_600/final libcxx
cd ../..

cd llvm/projects
svn co http://llvm.org/svn/llvm-project/libcxxabi/tags/RELEASE_600/final libcxxabi
cd ../..

mkdir build
cd build
#  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.9 -DCMAKE_C_FLAGS="-mmacosx-version-min=10.9" -DCMAKE_CXX_FLAGS="-mmacosx-version-min=10.9"
/cmake/bin/cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_INCLUDE_TOOLS=1 -DLLVM_BUILD_TOOLS=1 -DBUILD_SHARED_LIBS=0  -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_INCLUDE_EXAMPLES=0 -DLLVM_INCLUDE_TESTS=0 -DLLVM_ENABLE_CXX1Y=1 -DCMAKE_INSTALL_PREFIX=/usr/local ../llvm
/cmake/bin/cmake --build . -- -j$(nproc)
/cmake/bin/cmake --build . --target install
