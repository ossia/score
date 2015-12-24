set(NACL                        ON)

set(PLATFORM_EMBEDDED           ON)
set(PLATFORM_NAME               "PNaCl")
set(PLATFORM_TRIPLET            "pnacl")
set(PLATFORM_PREFIX             "/opt/nacl_sdk/pepper_42/toolchain/linux_pnacl")
set(PLATFORM_EXE_SUFFIX         ".pexe")

set(CMAKE_SYSTEM_NAME           "Linux" CACHE STRING "Target system.")
set(CMAKE_SYSTEM_PROCESSOR      "LLVM" CACHE STRING "Target processor.")
set(CMAKE_FIND_ROOT_PATH        "${PLATFORM_PREFIX}/le32-nacl")
set(CMAKE_AR                    "${PLATFORM_PREFIX}/bin/${PLATFORM_TRIPLET}-ar" CACHE STRING "")
set(CMAKE_RANLIB                "${PLATFORM_PREFIX}/bin/${PLATFORM_TRIPLET}-ranlib" CACHE STRING "")
set(CMAKE_C_COMPILER            "${PLATFORM_PREFIX}/bin/${PLATFORM_TRIPLET}-clang")
set(CMAKE_CXX_COMPILER          "${PLATFORM_PREFIX}/bin/${PLATFORM_TRIPLET}-clang++")
set(CMAKE_C_FLAGS               "-U__STRICT_ANSI__" CACHE STRING "")
set(CMAKE_CXX_FLAGS             "-std=c++14 -U__STRICT_ANSI__" CACHE STRING "")
set(CMAKE_C_FLAGS_RELEASE       "-Ofast -flto" CACHE STRING "")
set(CMAKE_CXX_FLAGS_RELEASE     "-std=c++14 -Ofast -flto" CACHE STRING "")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

include(CMakeForceCompiler)
cmake_force_c_compiler(${CMAKE_C_COMPILER} Clang)
cmake_force_cxx_compiler(${CMAKE_CXX_COMPILER} Clang)

macro(pnacl_finalise _target)
  add_custom_command(TARGET ${_target} POST_BUILD
    COMMENT "Finalising ${_target}"
    COMMAND "${PLATFORM_PREFIX}/bin/${PLATFORM_TRIPLET}-finalize" "$<TARGET_FILE:${_target}>")
endmacro()

include_directories(SYSTEM $ENV{NACL_SDK_ROOT}/include)
include_directories(SYSTEM $ENV{NACL_SDK_ROOT}/include/newlib)
# For QtCreator to find system includes.
include_directories(SYSTEM ${CMAKE_FIND_ROOT_PATH}/usr/include)
include_directories(SYSTEM ${CMAKE_FIND_ROOT_PATH}/include)
include_directories(SYSTEM ${CMAKE_FIND_ROOT_PATH}/include/c++/v1)

link_directories($ENV{NACL_SDK_ROOT}/lib/pnacl/Release)
