cmake_minimum_required(VERSION 3.21)
project(jitmin CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "jitmin: LLVM ${LLVM_PACKAGE_VERSION} from ${LLVM_DIR}")

# Shared with the score JIT plugin -- the exact same COFF platform implementation.
set(JIT_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src/plugins/score-plugin-jit")

add_executable(jitmin
  main.cpp
  "${JIT_PLUGIN_DIR}/JitCpp/Compiler/MinGWCOFFPlatform.cpp")

target_include_directories(jitmin PRIVATE
  "${JIT_PLUGIN_DIR}"          # so <JitCpp/Compiler/MinGWCOFFPlatform.hpp> resolves
  ${LLVM_INCLUDE_DIRS})

target_compile_definitions(jitmin PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(JITMIN_LLVM_LIBS
  OrcJIT JITLink OrcShared OrcTargetProcess
  Object BinaryFormat IRReader BitReader AsmParser
  Core Support TargetParser
  X86CodeGen X86AsmParser X86Desc X86Info)

target_link_libraries(jitmin PRIVATE ${JITMIN_LLVM_LIBS})
