project(iscore)
cmake_minimum_required(VERSION 3.1)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/CMake;${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules;${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules/cmake-modules")

set(ISCORE_VERSION_MAJOR 1)
set(ISCORE_VERSION_MINOR 0)
set(ISCORE_VERSION_PATCH 0)
set(ISCORE_VERSION_EXTRA a33)
set(ISCORE_CODENAME "Je suis un ananas")
set(ISCORE_VERSION "${ISCORE_VERSION_MAJOR}.${ISCORE_VERSION_MINOR}.${ISCORE_VERSION_PATCH}-${ISCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")

if(ISCORE_SANITIZE)
    set(OSSIA_SANITIZE True)
endif()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API")
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
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator")
add_subdirectory("${3RDPARTY_FOLDER}/QRecentFilesMenu")

set(ISCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins")

add_subdirectory(base)

#if(INTEGRATION_TESTING)

 #       include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Tests")
 #       file(GLOB_RECURSE TESTS_HDRS
 #               "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.hpp")
  #      add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
  #  setup_iscore_tests(Tests/Integration)
#endif()

#include(CTest)
