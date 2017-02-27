set(CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH}"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Configurations/travis"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/modules/cmake-modules"
  "${CMAKE_CURRENT_SOURCE_DIR}/API/CMake/cmake-modules"
  )
set(CMAKE_PREFIX_PATH
  "${CMAKE_PREFIX_PATH}"
  "${CMAKE_MODULE_PATH}")

if(NOT ISCORE_CONFIGURATION)
  set(ISCORE_CONFIGURATION developer)
endif()
include("${ISCORE_CONFIGURATION}")

project(iscore LANGUAGES CXX)

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0063 NEW)

set(ISCORE_VERSION_MAJOR 1)
set(ISCORE_VERSION_MINOR 0)
set(ISCORE_VERSION_PATCH 0)
set(ISCORE_VERSION_EXTRA b7)
set(ISCORE_CODENAME "i-pocrites")
set(ISCORE_VERSION "${ISCORE_VERSION_MAJOR}.${ISCORE_VERSION_MINOR}.${ISCORE_VERSION_PATCH}-${ISCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")

# OSSIA API-specific configuration
if(ISCORE_SANITIZE)
    set(OSSIA_SANITIZE True CACHE INTERNAL "" FORCE)
endif()
if(ISCORE_STATIC_PLUGINS)
    set(OSSIA_STATIC ON CACHE INTERNAL "" FORCE)
endif()
if(ISCORE_ENABLE_LTO)
    set(OSSIA_LTO ON CACHE INTERNAL "" FORCE)
endif()
set(OSSIA_NO_INSTALL ON CACHE INTERNAL "" FORCE)
set(OSSIA_PD OFF CACHE INTERNAL "" FORCE)
set(OSSIA_JAVA OFF CACHE INTERNAL "" FORCE)
set(OSSIA_OSX_FAT_LIBRARIES OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PYTHON OFF CACHE INTERNAL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API" )

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Gui Network Xml Svg Test Qml WebSockets)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/API/3rdparty")
set(ISCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(ISCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${ISCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/variant/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/brigand")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/hopscotch-map/src")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
add_subdirectory("${3RDPARTY_FOLDER}/Qt-Color-Widgets" )
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator" )
add_subdirectory("${3RDPARTY_FOLDER}/QRecentFilesMenu" )

set(WITH_DOC False CACHE INTERNAL "" FORCE)
set(ZMQ_BUILD_TESTS False CACHE INTERNAL "" FORCE)
#add_subdirectory("${OSSIA_3RDPARTY_FOLDER}/libzmq" )
#include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/cppzmq")

set(SKIP_INSTALL_ALL True) # For zlib to not install anything
add_subdirectory("${3RDPARTY_FOLDER}/zlib" EXCLUDE_FROM_ALL)
add_subdirectory("${3RDPARTY_FOLDER}/quazip" EXCLUDE_FROM_ALL)

set(ISCORE_ZEROCONF False)
find_package(KF5DNSSD QUIET NO_MODULE)
if(KF5DNSSD_FOUND)
    find_package(Qt5 5.3 COMPONENTS DBus)
    if(Qt5DBus_FOUND)
        set(ISCORE_ZEROCONF True)
    endif()
endif()

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
