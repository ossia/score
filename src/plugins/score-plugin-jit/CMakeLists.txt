project(score_plugin_jit LANGUAGES CXX)

# Find dependencies
find_package(LLVM PATHS ${LLVM_DIR})
if(${Qt5Core_VERSION_STRING} VERSION_LESS 5.15)
  message(" -- JIT plug-in only supported for Qt 5.15+")
  return()
endif()

if(NOT LLVM_FOUND)
  message(" -- LLVM was not found, skipping JIT plug-in")
  return()
endif()

if(LLVM_VERSION VERSION_GREATER "10.0")
  message(" -- LLVM version: ${LLVM_VERSION}")
else()
  message(" -- LLVM Version ${LLVM_VERSION} incompatible, skipping JIT plug-in")
  return()
endif()

find_package(Clang PATHS ${LLVM_DIR}/../Clang)
if(NOT Clang_FOUND)
  message(" -- libclang was not found, skipping JIT plug-in")
  return()
endif()

find_package(Polly PATHS ${LLVM_DIR}/../Polly)

# Based on the code in
# https://github.com/weliveindetail/JitFromScratch

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED 17)
# General initialization
score_common_setup()

# Source files
set(HDRS
    JitCpp/AddonCompiler.hpp
    JitCpp/EditScript.hpp
    JitCpp/ClangDriver.hpp
    JitCpp/JitModel.hpp
    JitCpp/JitOptions.hpp
    JitCpp/JitPlatform.hpp
    JitCpp/JitUtils.hpp
    JitCpp/ApplicationPlugin.hpp
    JitCpp/MetadataGenerator.hpp
    JitCpp/Compiler/Compiler.hpp
    JitCpp/Compiler/Driver.hpp

    Bytebeat/Bytebeat.hpp

    score_plugin_jit.hpp
)

set(SRCS
    JitCpp/AddonCompiler.cpp
    JitCpp/JitModel.cpp
    JitCpp/ApplicationPlugin.cpp

    Bytebeat/Bytebeat.cpp

    score_plugin_jit.cpp

    # Note: has to be last as it uses some macros that conflicts with Qt's
    # which we have to #undef, which can break unity builds
    JitCpp/ClangDriver.cpp
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
if(TARGET score_plugin_gfx)
  set(HDRS ${HDRS} Texgen/Texgen.hpp)
  target_sources(${PROJECT_NAME} PRIVATE Texgen/Texgen.hpp Texgen/Texgen.cpp)
  target_link_libraries(${PROJECT_NAME} PRIVATE score_plugin_gfx)
endif()

# LLVM definitions
separate_arguments(LLVM_DEFINITIONS)

# Project-specific definitions
target_include_directories(score_plugin_jit PUBLIC
    ${LLVM_INCLUDE_DIRS}
)
target_compile_definitions(score_plugin_jit PUBLIC
    ${LLVM_DEFINITIONS}
)

if("${LLVM_PACKAGE_VERSION}" MATCHES "(.*)svn")
  set(LLVM_PACKAGE_VERSION "${CMAKE_MATCH_1}")
endif()

target_compile_definitions(score_plugin_jit
  PUBLIC
    SCORE_LLVM_VERSION="${LLVM_PACKAGE_VERSION}"
    SCORE_ROOT_SOURCE_DIR="${SCORE_ROOT_SOURCE_DIR}"
    SCORE_ROOT_BINARY_DIR="${SCORE_ROOT_BINARY_DIR}"
)

target_compile_options(score_plugin_jit PRIVATE -std=c++17)

# Clang dependencies
if(LLVM_VERSION VERSION_LESS "10.*" OR (UNIX AND NOT APPLE AND DEPLOYMENT_BUILD) OR OSSIA_SDK)
  set(CLANG_LIBS
    clangARCMigrate
    clangBasic
    clangCodeGen
    clangDriver
    clangFrontend
    clangFrontendTool
    clangRewriteFrontend
    clangARCMigrate
    clangStaticAnalyzerFrontend
    clangParse
    clangSerialization
    clangSema
    clangEdit
    clangStaticAnalyzerCheckers
    clangASTMatchers
    clangStaticAnalyzerCore
    clangAnalysis
    clangAST
    clangRewrite
    clangLex
    clangCrossTU
    clangIndex
  )
else()
  set(CLANG_LIBS
      clang
      clang-cpp
  )
endif()

if(Polly_FOUND)
  set(POLLY_LIBS Polly PollyISL PollyPPCG)
endif()

llvm_map_components_to_libnames(LLVM_LIBS
  all
)

if("${LLVM_LIBS}" STREQUAL "")
  set(LLVM_LIBS "${LLVM_AVAILABLE_LIBS}")
  list(REMOVE_ITEM LLVM_LIBS LTO)
  list(REMOVE_ITEM LLVM_LIBS OptRemarks)
  list(REMOVE_ITEM LLVM_LIBS Remarks)
endif()

if("${LLVM_LIBS}" STREQUAL "")
  set(LLVM_LIBS LLVM)
endif()

list(FIND LLVM_LIBS LLVM LLVM_HAS_SHARED_LIBS)
if("${LLVM_HAS_SHARED_LIBS}" GREATER 0)
  set(LLVM_LIBS LLVM)
endif()

if(TARGET score_plugin_media)
  get_target_property(LIBS score_plugin_media LINK_LIBRARIES)
  list (FIND LIBS "/usr/local/lib/libfaust.so" _index)
  if (${_index} GREATER -1)
    set(LLVM_LIBS LLVM)
  endif()
endif()

if(WIN32)
  list(REMOVE_ITEM LLVM_LIBS LTO)
  list(REMOVE_ITEM LLVM_LIBS OptRemarks)
  target_link_libraries(score_plugin_jit PUBLIC -Wl,--start-group ${LLVM_LIBS} ${CLANG_LIBS} -Wl,--end-group mincore)
elseif(APPLE)
  target_link_libraries(score_plugin_jit PUBLIC ${CLANG_LIBS} ${POLLY_LIBS} ${LLVM_LIBS})
else()
  target_link_libraries(score_plugin_jit PUBLIC -Wl,--start-group ${CLANG_LIBS} ${POLLY_LIBS} ${LLVM_LIBS} -Wl,--end-group)
endif()

# Code generation
score_generate_command_list_file(${PROJECT_NAME} "${HDRS}")

# Link
target_link_libraries(${PROJECT_NAME} PUBLIC score_lib_process score_plugin_library score_plugin_engine)

# Target-specific options
setup_score_plugin(${PROJECT_NAME})
if(NOT WIN32)
  get_target_property(CF ${PROJECT_NAME} COMPILE_FEATURES)
  get_target_property(CXF ${PROJECT_NAME} COMPILE_OPTIONS)

  list(REMOVE_ITEM CF cxx_std_20)
  list(APPEND CF cxx_std_17)
  set_target_properties(${PROJECT_NAME}
    PROPERTIES
      COMPILE_FEATURES "${CF}"
      INTERFACE_COMPILE_FEATURES "${CF}"
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
  )
endif()
# Things to install :
# - lib/clang/${LLVM_PACKAGE_VERSION}
# - libc++
# - Qt headers
# - MinGW headers

#
# add_library(Benchmarker SHARED Benchmarks/Benchmarker.cpp)
# target_link_libraries(Benchmarker PUBLIC ${SCORE_PLUGINS_LIST} -Wl,--stack,16777216)
# set_target_properties(Benchmarker PROPERTIES ENABLE_EXPORTS 1)
# cotire(Benchmarker)
# install(TARGETS Benchmarker RUNTIME DESTINATION .)
# add_executable(bench_main Benchmarks/bench_main.cpp)
# target_link_libraries(bench_main PRIVATE Benchmarker -Wl,--stack,16777216)
#
# cotire(bench_main)
# install(TARGETS bench_main RUNTIME DESTINATION .)
#
