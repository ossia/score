# Options

option(ENABLE_LTO "Enable link-time optimization. Won't work on Travis." OFF)
option(ENABLE_OPTIMIZE_CUSTOM "Enable -march=native." OFF)

option(OSSIA_NO_EXAMPLES "Don't build OSSIA examples" True)
option(OSSIA_NO_TESTS "Don't build OSSIA tests" True)

option(ISCORE_COTIRE "Use cotire. Will make the build faster." OFF)
option(ISCORE_COTIRE_ALL_HEADERS "All headers will be put in prefix headers. Faster for CI but slower for development" OFF)

option(ISCORE_USE_DEV_PLUGINS "Build the prototypal plugins" OFF)

# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5

# Settings
cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)

set(ISCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ANDROID_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Android")
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules")


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Za /wd4180 /wd4224")
else()


  # Linux flags
  if(NOT APPLE AND NOT WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -Wl,-z,defs -fvisibility=hidden")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -Wl,-z,defs -fvisibility=hidden")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -Wl,-z,defs -fvisibility=hidden")

    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")

        if(ENABLE_LTO) # TODO PROPERTY INTERPROCEDURAL_OPTIMIZATION
            find_program(CMAKE_GCC_AR NAMES ${_CMAKE_TOOLCHAIN_PREFIX}gcc-ar${_CMAKE_TOOLCHAIN_SUFFIX} HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
            find_program(CMAKE_GCC_NM NAMES ${_CMAKE_TOOLCHAIN_PREFIX}gcc-nm HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
            find_program(CMAKE_GCC_RANLIB NAMES ${_CMAKE_TOOLCHAIN_PREFIX}gcc-ranlib HINTS ${_CMAKE_TOOLCHAIN_LOCATION})
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto -fno-fat-lto-objects " )
            set(CMAKE_AR "${CMAKE_GCC_AR}")
            set(CMAKE_NM "${CMAKE_GCC_NM}")
            set(CMAKE_RANLIB "${CMAKE_GCC_RANLIB}")
        endif()
    endif()
  endif()

  # OS X flags
  if(APPLE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast")
  endif()

  # Common Unix flags
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wabi -Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder -Wstrict-null-sentinel -Wno-non-template-friend -Woverloaded-virtual -Wno-pmf-conversions -Wsign-promo -Wextra -Wall -Waddress -Waggregate-return -Warray-bounds -Wno-attributes -Wno-builtin-macro-redefined")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wc++0x-compat -Wcast-align -Wcast-qual -Wchar-subscripts -Wclobbered -Wcomment -Wconversion -Wcoverage-mismatch -Wno-deprecated -Wno-deprecated-declarations -Wdisabled-optimization -Wno-div-by-zero -Wempty-body -Wenum-compare")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pipe -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas")

  if(ENABLE_OPTIMIZE_CUSTOM)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")
  endif()

  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")
endif()


if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DISCORE_DEBUG)
  add_definitions(-DBOOST_MULTI_INDEX_ENABLE_SAFE_MODE)
  add_definitions(-DBOOST_MULTI_INDEX_ENABLE_INVARIANT_CHECKING)

endif()

#if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weverything -Wno-c++98-compat -Wno-exit-time-destructors -Wno-padded")
#endif()

include(cotire)
