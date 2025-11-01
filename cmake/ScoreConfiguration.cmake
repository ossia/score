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
option(SCORE_CUSTOM_QT_PLUGINS "Set Qt plugins statically" "")
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
  # gcov/gcovr instrumentation for coveralls.io (see .github/workflows/coverage.yml).
  # Historically this included the Lars Bilke CodeCoverage.cmake module and
  # defined report targets via setup_target_for_coverage(); that module was
  # dropped when the project left Travis (it lived under the old CMake/modules/
  # path and was never carried over), leaving CMAKE_CXX_FLAGS_COVERAGE empty and
  # setup_target_for_coverage() undefined — i.e. the option was a silent no-op
  # that would error at configure time. We now inject the gcov flags directly;
  # gcovr generates the report (works with gcc's gcov and, for clang builds,
  # with `gcovr --gcov-executable "llvm-cov gcov"`).
  add_compile_options(-O0 -g --coverage -fprofile-arcs -ftest-coverage)
  add_link_options(--coverage)
endif()

# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5

# Settings
if(CMAKE_CROSSCOMPILING)
  set(SCORE_NATIVE_ARCH_FLAG "")
  set(SCORE_AVND_OPT_FLAGS "-O3;-g0")
else()
  set(SCORE_NATIVE_ARCH_FLAG "-march=native")
  set(SCORE_AVND_OPT_FLAGS "-O3;-march=native;-g0")
endif()

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

if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  # Define _FILE_OFFSET_BITS globally when the libc supports it: deps like
  # FFmpeg inject it through pkg-config into some targets only, and the
  # mismatch makes gcc reject the PCH in those targets.
  include(CheckCXXSourceCompiles)
  set(CMAKE_REQUIRED_DEFINITIONS -D_FILE_OFFSET_BITS=64)
  check_cxx_source_compiles("
#include <sys/types.h>
#include <cstdio>
static_assert(sizeof(off_t) == 8, \"off_t must be 64-bit\");
int main() { fseeko(nullptr, 0, 0); return 0; }
" SCORE_LIBC_LARGEFILE64)
  unset(CMAKE_REQUIRED_DEFINITIONS)
  if(SCORE_LIBC_LARGEFILE64)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
  endif()
endif()

set(CMAKE_ANDROID_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Android")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CXX_IS_CLANG True)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fbracket-depth=1024")

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
  set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} runtimeobject")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CXX_IS_GCC True)
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)

  if (GCC_VERSION VERSION_LESS 10)
    message(FATAL_ERROR "score requires at least g++-10 to build. ")
  endif()
endif()

if (NOT EMSCRIPTEN AND OSSIA_ENABLE_KFR)
  if(NOT ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8"))
    message(FATAL_ERROR
      "kfrlib only supports 64-bit systems. Configure with -DOSSIA_ENABLE_KFR=0 "
      "to build score for a 32-bit target without it.")
  endif()
endif()

check_cxx_compiler_flag(-Wno-gnu-anonymous-struct has_w_gnu_anonymous_struct_flag)
check_cxx_compiler_flag(-Wno-nested-anon-types has_w_nested_anon_types_flag)
check_cxx_compiler_flag(-Wno-dtor-name has_w_dtor_name_flag)
check_cxx_compiler_flag(-Wno-null-conversion has_w_null_conversion_flag)
check_cxx_compiler_flag(-Wno-unneeded-internal-declaration has_w_unneeded_internal_declaration_flag)
check_cxx_compiler_flag(-Wno-error=missing-exception-spec has_w_missing_exception_spec)
check_cxx_compiler_flag(-Wno-sign-compare has_w_sign_compare)
check_cxx_compiler_flag(-Wc2y-extensions has_w_c2y_extensions)

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

if(has_w_c2y_extensions)
  add_compile_options(-Wno-c2y-extensions)
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
  if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-clang:-fopenmp-simd)
  else()
    add_compile_options(-openmp:experimental)
  endif()
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

if(WIN32)
  # Pin the Windows target for every translation unit, whatever the compiler.
  #
  # <windows.h> defines _WIN32_WINNT itself, through <sdkddkver.h>, whenever it
  # is not already set, so leaving it alone does not mean "no minimum" - it
  # means each translation unit gets one depending on whether it reached
  # <windows.h> at all. Headers that branch on it, Boost.Asio among them, then
  # configure themselves differently from one file to the next, which is an ODR
  # violation that nothing reports, because both spellings mangle the same.
  #
  # The defaults also disagree between toolchains: the Windows Kits header
  # picks 0x0A00 while mingw-w64 picks _WIN32_WINNT_WS03, so a mingw build has
  # been configuring itself for Server 2003 wherever this was left alone.
  #
  # WINVER and NTDDI_VERSION are deliberately not set: sdkddkver.h derives
  # WINVER from _WIN32_WINNT and NTDDI_VERSION from the SDK, in both
  # toolchains, so setting them by hand only creates a way for them to
  # disagree.
  #
  # Global rather than PUBLIC on a target because the dependencies that compile
  # Asio are siblings of the libraries consuming it rather than consumers
  # themselves: libremidi builds its own translation units on MSVC, and a
  # PUBLIC definition on ossia never reaches them. libossia repeats this in its
  # own top-level CMakeLists so it is correct when built standalone.
  # And Asio's version namespace, which libossia enables and which is part of
  # its ABI: the namespace is an inline namespace, so it is baked into the
  # mangled name of every Asio type its headers expose. libossia explicitly
  # instantiates resolve_sync_v4 for boost::asio::ip::udp and ::tcp, so a
  # translation unit here that does not agree about the namespace names a
  # different specialisation and fails to link. It comes with the ossia target
  # too; it is repeated here for the targets that reach Asio without linking
  # ossia.
  add_definitions(
    -D_WIN32_WINNT=0x0A00
    -DBOOST_ASIO_ENABLE_VERSION_NAMESPACE=1
  )

  # NOMINMAX and WIN32_LEAN_AND_MEAN belong here too, rather than in the MSVC
  # branch of the top-level CMakeLists where they used to sit: what they change
  # is what <windows.h> declares, which is a property of the platform and not of
  # the compiler. Left to individual targets they reached about three quarters
  # of the build, so whether min and max were macros depended on which target a
  # header happened to be compiled into. No value, which is the spelling
  # libossia uses and the one every consistent translation unit here already
  # had.
  #
  # C++ only. Neither changes a type layout - they change which declarations are
  # visible - so the consistency that matters is between the translation units
  # that share C++ types, and the vendored C we build has been written against
  # the unabridged <windows.h>. libpd's pd~.c reaches malloc and errno through
  # it, and stops compiling on the toolchains that reject undeclared library
  # functions once the lean header is imposed on it.
  add_compile_definitions(
    $<$<COMPILE_LANGUAGE:CXX>:NOMINMAX>
    $<$<COMPILE_LANGUAGE:CXX>:WIN32_LEAN_AND_MEAN>
  )
endif()

# Boost changes what it declares depending on this - typeid use, and with it the
# layout of the types that carry a std::type_info around - so it has to be the
# same everywhere or two translation units disagree about a Boost class while
# spelling its name identically. libossia sets it PUBLIC on the ossia target,
# which covered what links ossia and left the rest of the build without it. Not
# guarded by WIN32: nothing about it is Windows-specific.
add_definitions(-DBOOST_NO_RTTI=1)

# Win32 codepage stuff
if(WIN32)
  file(CONFIGURE
    OUTPUT
      "${CMAKE_BINARY_DIR}/score.exe.manifest"
    CONTENT
    [=[<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
    <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
      <application xmlns="urn:schemas-microsoft-com:asm.v3">
         <windowsSettings> <dpiAware       xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true/PM</dpiAware>                     </windowsSettings>
         <windowsSettings> <dpiAwareness   xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">PerMonitorV2,PerMonitor</dpiAwareness> </windowsSettings>
         <windowsSettings> <longPathAware  xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>                   </windowsSettings>
         <windowsSettings> <activeCodePage xmlns="http://schemas.microsoft.com/SMI/2019/WindowsSettings">UTF-8</activeCodePage>                 </windowsSettings>
      </application>
      <trustInfo xmlns="urn:schemas-microsoft-com:asm.v2">
        <security>
          <requestedPrivileges xmlns="urn:schemas-microsoft-com:asm.v3">
            <requestedExecutionLevel level="asInvoker" uiAccess="false" />
          </requestedPrivileges>
        </security>
      </trustInfo>
      <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
        <application>
          <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}" />
          <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}" />
          <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}" />
          <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}" />
        </application>
      </compatibility>
    </assembly>
    ]=]
  )

  if(MINGW)
    enable_language(RC)
    # 1 = CREATEPROCESS_MANIFEST_RESOURCE_ID, 24 = RT_MANIFEST (no includes needed)
    file(WRITE "${CMAKE_BINARY_DIR}/score.exe.rc"
         "1 24 \"${CMAKE_BINARY_DIR}/score.exe.manifest\"\n")
  endif()
endif()

function(target_enable_utf8 tgt)
  if(NOT SCORE_MSSTORE_DEPLOYMENT) # c.f. ScoreDeploymentWindowsStore
    if(MSVC)
      target_link_options(${tgt} PRIVATE
        /MANIFEST:EMBED
        "/MANIFESTINPUT:${CMAKE_BINARY_DIR}/score.exe.manifest")
    elseif(MINGW)
      # Compile the manifest resource in a dedicated, flag-free object library.
      # Adding score.exe.rc straight to ${tgt} makes windres inherit the target's
      # hundreds of -I/-D flags and overflow the Windows command-line length limit
      # ("windres: preprocessing failed"). The resource (`1 24 "…manifest"`) needs
      # no include dirs or defines at all, so strip them.
      if(NOT TARGET score_win_manifest)
        add_library(score_win_manifest OBJECT "${CMAKE_BINARY_DIR}/score.exe.rc")
        set_target_properties(score_win_manifest PROPERTIES
          INCLUDE_DIRECTORIES ""
          COMPILE_DEFINITIONS "")
      endif()
      target_sources(${tgt} PRIVATE $<TARGET_OBJECTS:score_win_manifest>)
    endif()
  endif()
endfunction()