set(CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH}"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations/travis"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules/cmake-modules")
set(CMAKE_PREFIX_PATH
  "${CMAKE_PREFIX_PATH}"
  "${CMAKE_MODULE_PATH}")

if(NOT ISCORE_CONFIGURATION)
  set(ISCORE_CONFIGURATION developer)
endif()
include("${ISCORE_CONFIGURATION}")
message(STATUS "iscore - Static plug-ins 0: ${ISCORE_STATIC_PLUGINS}")

project(iscore LANGUAGES CXX)
message(STATUS "iscore - Static plug-ins 1: ${ISCORE_STATIC_PLUGINS}")

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0063 NEW)

set(ISCORE_VERSION_MAJOR 1)
set(ISCORE_VERSION_MINOR 0)
set(ISCORE_VERSION_PATCH 0)
set(ISCORE_VERSION_EXTRA a71)
set(ISCORE_CODENAME "On nie la notion de temps")
set(ISCORE_VERSION "${ISCORE_VERSION_MAJOR}.${ISCORE_VERSION_MINOR}.${ISCORE_VERSION_PATCH}-${ISCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")
message(STATUS "iscore - Static plug-ins 2: ${ISCORE_STATIC_PLUGINS}")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")
message(STATUS "iscore - Static plug-ins 3: ${ISCORE_STATIC_PLUGINS}")
if(ISCORE_SANITIZE)
    set(OSSIA_SANITIZE True)
endif()
if(ISCORE_STATIC_PLUGINS)
    set(OSSIA_STATIC ON)
endif()
set(OSSIA_NO_INSTALL ON)
message(STATUS "iscore - Static plug-ins 4: ${ISCORE_STATIC_PLUGINS} ${OSSIA_STATIC}")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API" )

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Gui Network Xml Svg Test Qml WebSockets)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(ISCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ISCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${ISCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/variant/include")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/verdigris/src")
add_subdirectory("${3RDPARTY_FOLDER}/Qt-Color-Widgets" )
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator" )
add_subdirectory("${3RDPARTY_FOLDER}/QRecentFilesMenu" )

set(SKIP_INSTALL_ALL True) # For zlib to not install anything
add_subdirectory("${3RDPARTY_FOLDER}/zlib" EXCLUDE_FROM_ALL)
add_subdirectory("${3RDPARTY_FOLDER}/quazip" EXCLUDE_FROM_ALL)

set(ISCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins") # todo libraries, components ?
set(ISCORE_GLOBAL_COMPONENTS_LIST CACHE INTERNAL "List of components") # todo libraries, components ?

add_subdirectory(base)
set(ISCORE_TESTBED OFF)
if(ISCORE_TESTBED)
add_subdirectory(Tests/testbed)
endif()
#if(INTEGRATION_TESTING)

 #       include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Tests")
 #       file(GLOB_RECURSE TESTS_HDRS
 #               "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.hpp")
  #      add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
  #  setup_iscore_tests(Tests/Integration)
#endif()

#include(CTest)
