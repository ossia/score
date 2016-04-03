set(CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH}"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations/travis"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules/cmake-modules")

if(NOT ISCORE_CONFIGURATION)
  set(ISCORE_CONFIGURATION developer)
endif()
include("${ISCORE_CONFIGURATION}")

project(iscore LANGUAGES CXX)
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)


set(ISCORE_VERSION_MAJOR 1)
set(ISCORE_VERSION_MINOR 0)
set(ISCORE_VERSION_PATCH 0)
set(ISCORE_VERSION_EXTRA a58)
set(ISCORE_CODENAME "Jim Jim Jimmy")
set(ISCORE_VERSION "${ISCORE_VERSION_MAJOR}.${ISCORE_VERSION_MINOR}.${ISCORE_VERSION_PATCH}-${ISCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")

if(ISCORE_SANITIZE)
    set(OSSIA_SANITIZE True)
endif()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API" EXCLUDE_FROM_ALL)
if(POLICY CMP0063)
CMAKE_POLICY(SET CMP0063 NEW)
endif()

find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Gui Network Xml Svg Test Qml)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(ISCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ISCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${ISCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/variant/include")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/nano-signal-slot")
add_subdirectory("${3RDPARTY_FOLDER}/Qt-Color-Widgets" EXCLUDE_FROM_ALL)
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator" EXCLUDE_FROM_ALL)
add_subdirectory("${3RDPARTY_FOLDER}/QRecentFilesMenu" EXCLUDE_FROM_ALL)

set(ISCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins") # todo libraries, components ?
set(ISCORE_GLOBAL_COMPONENTS_LIST CACHE INTERNAL "List of components") # todo libraries, components ?

add_subdirectory(base)
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
