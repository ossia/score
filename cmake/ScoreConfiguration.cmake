include(CheckCXXCompilerFlag)
include(CheckLinkerFlag)
include(CheckCXXSymbolExists)

# Options

if(UNIX AND NOT APPLE)
    find_program(LSB_RELEASE lsb_release)
    if(LSB_RELEASE)
      execute_process(COMMAND ${LSB_RELEASE} -i
          OUTPUT_VARIABLE RELEASE_CODENAME
          OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
endif()

option(SCORE_PCH "Use precompiled headers. Will make the build faster." OFF)

option(INTEGRATION_TESTING "Run integration tests" OFF)

option(SCORE_BUILD_FOR_PACKAGE_MANAGER "Set FHS-friendly install paths, plugins will go in /usr/lib/score/lib<blabla>.so" OFF)

option(SCORE_IEEE "Use a graphical skin adapted to publication" OFF)
option(SCORE_WEBSOCKETS "Run a websocket server in the scenario" OFF)
option(SCORE_TESTBED "Enable the testbed. See Tests/testbed/README" OFF)
option(SCORE_PLAYER "Build standalone player" OFF)
option(SCORE_FHS_BUILD "For installing in Linux distros /usr hierarchy" OFF)
option(SCORE_USE_SYSTEM_LIBRARIES "Try to use system libraries as far as possible" OFF)
option(DEFINE_SCORE_SCENARIO_DEBUG_RECTS "Enable to have debug rects around elements of a scenario" OFF)

option(SCORE_COVERAGE "Enable coverage" OFF)
option(SCORE_ENABLE_CXX26 "Enable c++26" OFF)

option(SCORE_INSTALL_HEADERS "Install headers" OFF)

option(SCORE_FAST_DEV_BUILD "Disables some features for faster development" OFF)
set(CMAKE_DEBUG_POSTFIX "")
if(APPLE)
  set(SCORE_OPENGL ON)
endif()

find_package(${QT_VERSION} COMPONENTS Core)
get_target_property(Qt_LibType ${QT_PREFIX}::Core TYPE)

if("${Qt_LibType}" STREQUAL "STATIC_LIBRARY")
  set(SCORE_STATIC_QT ON CACHE INTERNAL "")
  set(SCORE_STATIC_PLUGINS True)
endif()

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x0609ff)
add_definitions(-DQT_NO_KEYWORDS)

if(UNIX AND NOT APPLE AND SCORE_DEPLOYMENT_BUILD)
  set(SCORE_BUILD_FOR_PACKAGE_MANAGER ON)
endif()

if(INTEGRATION_TESTING)
  set(SCORE_STATIC_PLUGINS True)
endif()

if(SCORE_STATIC_PLUGINS)
  set(BUILD_SHARED_LIBS OFF)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

if(SCORE_COVERAGE)
  include("${CMAKE_CURRENT_LIST_DIR}/modules/CodeCoverage.cmake")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_COVERAGE}")
  set(CMAKE_EXE_LINKER_FLAGS_COVERAGE "${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_COVERAGE}")
  set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE "${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_COVERAGE}")
endif()

# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5

# Settings
include(ProcessorCount)
include(GenerateStaticExport)

check_cxx_symbol_exists(_LIBCPP_VERSION version LLVM_LIBCXX)
check_cxx_symbol_exists(__GLIBCXX__ version GNU_LIBSTDCXX)

if(UNIX AND NOT APPLE AND NOT SCORE_STATIC_PLUGINS AND SCORE_DEPLOYMENT_BUILD)
  set(CMAKE_INSTALL_RPATH "plugins")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

if(SCORE_STATIC_PLUGINS)
  set(CMAKE_SKIP_BUILD_RPATH TRUE)
endif()

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY True)
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_ANDROID_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Android")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CXX_IS_CLANG True)

    if(("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL "16") AND ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS "20") AND LLVM_LIBCXX)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library")
    elseif(APPLE)
      if(NOT x86_64 IN_LIST CMAKE_OSX_ARCHITECTURES)
        # In XCode 15.2 / macos-13 it causes an error in <chrono> due to
        # including <to_chars> only available from macos 13.3
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexperimental-library")
      endif()
    endif()
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
  set(CXX_IS_MSVC True)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} runtimeobject.lib")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
elseif(MINGW)
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lruntimeobject")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CXX_IS_GCC True)
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)

  if (GCC_VERSION VERSION_LESS 10)
    message(FATAL_ERROR "score requires at least g++-10 to build. ")
  endif()
endif()

if (NOT EMSCRIPTEN)
  if(NOT ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8"))
    message(FATAL_ERROR "score depends on kfrlib which only supports 64-bit systems.")
  endif()
endif()

check_cxx_compiler_flag(-Wno-gnu-anonymous-struct has_w_gnu_anonymous_struct_flag)
check_cxx_compiler_flag(-Wno-nested-anon-types has_w_nested_anon_types_flag)
check_cxx_compiler_flag(-Wno-dtor-name has_w_dtor_name_flag)
check_cxx_compiler_flag(-Wno-null-conversion has_w_null_conversion_flag)
check_cxx_compiler_flag(-Wno-unneeded-internal-declaration has_w_unneeded_internal_declaration_flag)
check_cxx_compiler_flag(-Wno-error=missing-exception-spec has_w_missing_exception_spec)
check_cxx_compiler_flag(-Wno-sign-compare has_w_sign_compare)

if(has_w_gnu_anonymous_struct_flag)
  add_compile_options(-Wno-gnu-anonymous-struct)
endif()

if(has_w_nested_anon_types_flag)
  add_compile_options(-Wno-nested-anon-types)
endif()

if(has_w_dtor_name_flag)
  add_compile_options(-Wno-dtor-name)
endif()

if(has_w_null_conversion_flag)
  add_compile_options(-Wno-null-conversion)
endif()

if(has_w_missing_exception_spec)
  add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Wno-error=missing-exception-spec>)
endif()

if(has_w_sign_compare)
  add_compile_options(-Wno-sign-compare)
endif()

check_cxx_compiler_flag(-std=c++26 has_std_26_flag)
check_cxx_compiler_flag(-std=c++2c has_std_2c_flag)
check_cxx_compiler_flag(-std=c++23 has_std_23_flag)

if (has_std_26_flag AND SCORE_ENABLE_CXX26)
  set(CXX_VERSION_FLAG cxx_std_26)
elseif (has_std_2c_flag AND SCORE_ENABLE_CXX26)
  set(CXX_VERSION_FLAG cxx_std_26)
else ()
  set(CXX_VERSION_FLAG cxx_std_23)
endif ()

check_cxx_compiler_flag(-fopenmp-simd has_fopenmp_simd_flag)
if(MSVC)
  add_compile_options(-openmp:experimental)
elseif(has_fopenmp_simd_flag)
  add_compile_options(-fopenmp-simd)
endif()

if(UNIX AND NOT APPLE AND NOT WIN32 AND NOT EMSCRIPTEN)
  check_linker_flag(CXX "LINKER:-zexecstack" has_zexecstack_flag)
  if(has_zexecstack_flag)
    add_link_options(-Wl,-zexecstack)
  endif()
endif()

# https://github.com/llvm/llvm-project/issues/131007
if(WIN32 AND ("${CMAKE_CXX_COMPILER_ID}" MATCHES Clang) AND ("${CMAKE_CXX_COMPILER_VERSION}" VERSION_GREATER_EQUAL "20"))
  set(CMAKE_C_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--whole-archive"
                                             "<LINK_ITEM>"
                                             "LINKER:--no-whole-archive")
  set(CMAKE_CXX_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--whole-archive"
                                             "<LINK_ITEM>"
                                             "LINKER:--no-whole-archive")
  set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--whole-archive"
                                             "<LINK_ITEM>"
                                             "LINKER:--no-whole-archive")

  set(_CMAKE_CXX_LINKER_PUSHPOP_STATE_SUPPORTED FALSE CACHE INTERNAL "linker supports push/pop state")
  set(_CMAKE_LINKER_PUSHPOP_STATE_SUPPORTED FALSE CACHE INTERNAL "linker supports push/pop state")
  set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED TRUE)
endif()

if(EMSCRIPTEN)
  # Ported from Modules/Platform/Linux.cmake
  # Features for LINK_LIBRARY generator expression
  ## check linker capabilities
  if(NOT DEFINED _CMAKE_LINKER_PUSHPOP_STATE_SUPPORTED)
    execute_process(COMMAND "${CMAKE_LINKER}" --help
                    OUTPUT_VARIABLE __linker_help
                    ERROR_VARIABLE __linker_help)
    if(__linker_help MATCHES "--push-state" AND __linker_help MATCHES "--pop-state")
      set(_CMAKE_LINKER_PUSHPOP_STATE_SUPPORTED TRUE CACHE INTERNAL "linker supports push/pop state")
    else()
      set(_CMAKE_LINKER_PUSHPOP_STATE_SUPPORTED FALSE CACHE INTERNAL "linker supports push/pop state")
    endif()
    unset(__linker_help)
  endif()

  ## WHOLE_ARCHIVE: Force loading all members of an archive
  if(_CMAKE_LINKER_PUSHPOP_STATE_SUPPORTED)
    set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--push-state,--whole-archive"
                                               "<LINK_ITEM>"
                                               "LINKER:--pop-state" CACHE "" INTERNAL)
  else()
    set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE "LINKER:--whole-archive"
                                               "<LINK_ITEM>"
                                               "LINKER:--no-whole-archive"  CACHE "" INTERNAL)
  endif()
  set(CMAKE_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED TRUE)

  # Features for LINK_GROUP generator expression
  ## RESCAN: request the linker to rescan static libraries until there is
  ## no pending undefined symbols
  set(CMAKE_LINK_GROUP_USING_RESCAN "LINKER:--start-group" "LINKER:--end-group")
  set(CMAKE_LINK_GROUP_USING_RESCAN_SUPPORTED TRUE)
endif()

if(CMAKE_UNITY_BUILD)
  set(SCORE_PCH 0)
endif()

check_cxx_compiler_flag("-Werror -Wextra -Wall -mcx16" has_mcx16_flag)
if(has_mcx16_flag)
  add_compile_options(-mcx16)
endif()

# Detect usage of asan / ubsan
if("${CMAKE_CXX_FLAGS}" MATCHES ".*sanitize.*")
  set(SCORE_HAS_SANITIZERS 1)
endif()

# Commit and version information
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  include(GetGitRevisionDescription)
  get_git_head_revision(GIT_COMMIT_REFSPEC GIT_COMMIT_HASH)
else()
  set(GIT_COMMIT_HASH "")
endif()
set(SCORE_VERSION_TAG "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}")
if(NOT "${SCORE_VERSION_EXTRA}" STREQUAL "")
  set(SCORE_VERSION_TAG "${SCORE_VERSION_TAG}-${SCORE_VERSION_EXTRA}")
endif()

score_write_file("${CMAKE_CURRENT_BINARY_DIR}/score_git_info.hpp"
"#pragma once
#define GIT_COMMIT \"${GIT_COMMIT_HASH}\"
#define SCORE_VERSION_MAJOR ${SCORE_VERSION_MAJOR}
#define SCORE_VERSION_MINOR ${SCORE_VERSION_MINOR}
#define SCORE_VERSION_PATCH ${SCORE_VERSION_PATCH}
#define SCORE_VERSION_EXTRA \"${SCORE_VERSION_EXTRA}\"
#define SCORE_CODENAME \"${SCORE_CODENAME}\"
#define SCORE_TAG \"v${SCORE_VERSION_TAG}\"
#define SCORE_TAG_NO_V \"${SCORE_VERSION_TAG}\"
")

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/score_compiler_detection.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/score_git_info.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/score_licenses.hpp"
    "${CMAKE_CURRENT_BINARY_DIR}/score_static_plugins.hpp"
  DESTINATION include/score
  COMPONENT Devel
  OPTIONAL)
