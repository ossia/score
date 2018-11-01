#!/bin/sh

LLVM_VER="RELEASE_700"

cd /image
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
