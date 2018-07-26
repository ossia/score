include(CheckCXXCompilerFlag)

# Options
CHECK_CXX_COMPILER_FLAG("-Wl,-z,defs" WL_ZDEFS_SUPPORTED)
CHECK_CXX_COMPILER_FLAG("-fuse-ld=gold" GOLD_LINKER_SUPPORTED)

if(UNIX AND NOT APPLE)
    find_program(LSB_RELEASE lsb_release)
    if(LSB_RELEASE)
      execute_process(COMMAND ${LSB_RELEASE} -i
          OUTPUT_VARIABLE RELEASE_CODENAME
          OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    endif()
endif()

option(SCORE_ENABLE_LTO "Enable link-time optimization. Won't work on Travis." OFF)
option(SCORE_ENABLE_OPTIMIZE_CUSTOM "Enable -march=native." OFF)

option(OSSIA_NO_EXAMPLES "Don't build OSSIA examples" True)
option(OSSIA_NO_TESTS "Don't build OSSIA tests" True)

option(SCORE_COTIRE "Use cotire. Will make the build faster." OFF)
option(SCORE_COTIRE_ALL_HEADERS "All headers will be put in prefix headers. Faster for CI but slower for development" OFF)

option(SCORE_STATIC_QT "Try to link with a static Qt" OFF)
option(SCORE_STATIC_EVERYTHING "Try to link with everything static" OFF)
option(SCORE_USE_DEV_PLUGINS "Build the prototypal plugins" OFF)
option(SCORE_SANITIZE "Build with sanitizers and debug glibc" OFF)
option(INTEGRATION_TESTING "Run integration tests" OFF)

option(SCORE_BUILD_FOR_PACKAGE_MANAGER "Set FHS-friendly install paths" OFF)

option(SCORE_IEEE "Use a graphical skin adapted to publication" OFF)
option(SCORE_WEBSOCKETS "Run a websocket server in the scenario" OFF)
option(SCORE_TESTBED "Enable the testbed. See Tests/testbed/README" OFF)
option(SCORE_PLAYER "Build standalone player" OFF)
option(DEFINE_SCORE_SCENARIO_DEBUG_RECTS "Enable to have debug rects around elements of a scenario" OFF)

option(SCORE_SPLIT_DEBUG "Split debug information" ON)
option(SCORE_COVERAGE "Enable coverage" OFF)

include("${SCORE_CONFIGURATION}")

set(CMAKE_DEBUG_POSTFIX "")
if(APPLE)
    set(SCORE_ADDON_PLATFORM "darwin-amd64")
    set(SCORE_ADDON_SUFFIX "amd64.dylib")
    set(SCORE_OPENGL ON)
elseif(WIN32)
    set(SCORE_ADDON_PLATFORM "windows-x86")
    set(SCORE_ADDON_SUFFIX "x86.dll")
elseif(UNIX)
    set(SCORE_ADDON_PLATFORM "linux-amd64")
    set(SCORE_ADDON_SUFFIX "amd64.so")
endif()

if(SCORE_STATIC_EVERYTHING)
  set(SCORE_STATIC_QT True)
  if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
  endif()
endif()

if(NACL)
  set(SCORE_STATIC_QT True)
endif()


# broken in ubuntu 17.10
if(NOT "${RELEASE_CODENAME}" MATCHES "Ubuntu")
  CHECK_CXX_COMPILER_FLAG("-fuse-ld=lld" LLD_LINKER_SUPPORTED)
endif()
if(SCORE_SANITIZE AND NOT APPLE)
  set(LLD_LINKER_SUPPORTED 0)
endif()

add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050800)
add_definitions(-DQT_NO_KEYWORDS)
if(SCORE_STATIC_QT)
  if(UNIX AND NOT APPLE)
    set(SCORE_STATIC_PLUGINS True)
    add_definitions(-DQT_STATIC)
    add_definitions(-DSCORE_STATIC_QT)
  endif()
endif()

if(ANDROID)
  set(SCORE_STATIC_PLUGINS True)
  set(Boost_FOUND True)
  include_directories("/opt/android-toolchain/arm-linux-androideabi/include")
else()
  find_package(Boost REQUIRED)
  include_directories(SYSTEM "${Boost_INCLUDE_DIRS}")
endif()

if(UNIX AND NOT APPLE AND DEPLOYMENT_BUILD)
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
include(GenerateExportHeader)

if(UNIX AND NOT APPLE AND NOT SCORE_STATIC_PLUGINS AND DEPLOYMENT_BUILD)
  set(CMAKE_INSTALL_RPATH "plugins")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY True)
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)

set(CMAKE_ANDROID_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Android")
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CXX_IS_CLANG True)
endif()

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(CXX_IS_MSVC True)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} runtimeobject.lib")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /bigobj")
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CXX_IS_GCC True)
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)

  if (GCC_VERSION VERSION_LESS 7)
    message(FATAL_ERROR "score requires at least g++-7 to build. ")
  endif()
endif()

if(SCORE_ENABLE_LTO)
  setup_lto()
endif()

# Useful header files
include(WriteCompilerDetectionHeader)
write_compiler_detection_header(
  FILE score_compiler_detection.hpp
  PREFIX SCORE
  COMPILERS GNU Clang AppleClang MSVC
  FEATURES cxx_relaxed_constexpr
  VERSION 3.1
)

# Commit and version information
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  include(GetGitRevisionDescription)
  get_git_head_revision(GIT_COMMIT_REFSPEC GIT_COMMIT_HASH)
else()
  set(GIT_COMMIT_HASH "")
endif()
score_write_file("${CMAKE_CURRENT_BINARY_DIR}/score_git_info.hpp"
"#pragma once
#define GIT_COMMIT \"${GIT_COMMIT_HASH}\"
#define SCORE_VERSION_MAJOR ${SCORE_VERSION_MAJOR}
#define SCORE_VERSION_MINOR ${SCORE_VERSION_MINOR}
#define SCORE_VERSION_PATCH ${SCORE_VERSION_PATCH}
#define SCORE_VERSION_EXTRA \"${SCORE_VERSION_EXTRA}\"
#define SCORE_CODENAME \"${SCORE_CODENAME}\"
")

set(COTIRE_UNITY_MAXIMUM_NUMBER_OF_INCLUDES "-j2")
include(cotire)
