project(iscore)
cmake_minimum_required(VERSION 3.0)


# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5

# Settings
# TODO Check Cmake options syntax
cmake_policy(SET CMP0042 NEW)
set(CTEST_OUTPUT_ON_FAILURE ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ANDROID_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Android")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules")

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wabi -Wctor-dtor-privacy -Wnon-virtual-dtor -Wreorder -Wstrict-null-sentinel -Wno-non-template-friend -Woverloaded-virtual -Wno-pmf-conversions -Wsign-promo -Wextra -Wall -Waddress -Waggregate-return -Warray-bounds -Wno-attributes -Wno-builtin-macro-redefined")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wc++0x-compat -Wcast-align -Wcast-qual -Wchar-subscripts -Wclobbered -Wcomment -Wconversion -Wcoverage-mismatch -Wno-deprecated -Wno-deprecated-declarations -Wdisabled-optimization -Wno-div-by-zero -Wempty-body -Wenum-compare")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -pipe -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas")
  if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}  -Wl,-z,defs")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -Wl,-z,defs")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -Wl,-z,defs")
  endif()
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
  endif()
endif()

#add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API")

if(ANDROID)
  set(ISCORE_STATIC_PLUGINS True)
  set(Boost_FOUND True)
  include_directories("/opt/android-toolchain/arm-linux-androideabi/include")
else()
  find_package(Boost)
  include_directories("${Boost_INCLUDE_DIRS}")
endif()

if(APPLE AND DEPLOYMENT_BUILD)
  set(ISCORE_STATIC_PLUGINS True)
endif()

if(ISCORE_STATIC_PLUGINS)
  set(BUILD_SHARED_LIBS OFF)
    add_definitions(-DQT_STATICPLUGIN)
else()
  set(BUILD_SHARED_LIBS ON)
endif()

add_subdirectory(base)
