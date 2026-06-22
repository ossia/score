cmake_minimum_required(VERSION 3.20)
project(jit_repro CXX)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_VERSION} from ${LLVM_DIR}")

add_executable(jit_repro jit_repro.cpp)
set_property(TARGET jit_repro PROPERTY CXX_STANDARD 20)
target_include_directories(jit_repro PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_definitions(jit_repro PRIVATE ${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs
  core support irreader orcjit jitlink executionengine
  target mc x86 native)
target_link_libraries(jit_repro PRIVATE ${llvm_libs})
