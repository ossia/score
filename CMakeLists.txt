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

if(NOT SCORE_CONFIGURATION)
  set(SCORE_CONFIGURATION developer)
endif()
include("${SCORE_CONFIGURATION}")

project(score LANGUAGES C CXX)

cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
cmake_policy(VERSION 3.9)

set(SCORE_VERSION_MAJOR 1)
set(SCORE_VERSION_MINOR 0)
set(SCORE_VERSION_PATCH 0)
set(SCORE_VERSION_EXTRA b38)
set(SCORE_CODENAME "peau crite")
set(SCORE_VERSION "${SCORE_VERSION_MAJOR}.${SCORE_VERSION_MINOR}.${SCORE_VERSION_PATCH}-${SCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")

# OSSIA API-specific configuration
if(SCORE_SANITIZE)
    set(OSSIA_SANITIZE True CACHE INTERNAL "" FORCE)
endif()
if(SCORE_SPLIT_DEBUG)
  set(OSSIA_SPLIT_DEBUG True CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_SPLIT_DEBUG False CACHE INTERNAL "" FORCE)
endif()
if(SCORE_COTIRE)
  set(OSSIA_DISABLE_COTIRE False CACHE INTERNAL "" FORCE)
else()
  set(OSSIA_DISABLE_COTIRE True CACHE INTERNAL "" FORCE)
endif()
if(SCORE_STATIC_PLUGINS)
    set(OSSIA_STATIC ON CACHE INTERNAL "" FORCE)
endif()
if(SCORE_ENABLE_LTO)
    set(OSSIA_LTO ON CACHE INTERNAL "" FORCE)
endif()
set(OSSIA_NO_INSTALL ON CACHE INTERNAL "" FORCE)
set(OSSIA_PD OFF CACHE INTERNAL "" FORCE)
set(OSSIA_JAVA OFF CACHE INTERNAL "" FORCE)
set(OSSIA_OSX_FAT_LIBRARIES OFF CACHE INTERNAL "" FORCE)
set(OSSIA_PYTHON OFF CACHE INTERNAL "" FORCE)
set(OSSIA_QT ON CACHE INTERNAL "" FORCE)
set(OSSIA_DISABLE_QT_PLUGIN ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_WEBSOCKETS ON CACHE INTERNAL "" FORCE)
set(OSSIA_PROTOCOL_SERIAL ON CACHE INTERNAL "" FORCE)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest")
  add_definitions(/D_HAS_AUTO_PTR_ETC=1 /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS=1)
else()
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
endif()
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API" )
#set(COTIRE_DEBUG 1)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Gui Network Xml Svg Test Qml WebSockets)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(OSSIA_3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/API/3rdparty")
set(SCORE_ROOT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(SCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${SCORE_ROOT_BINARY_DIR}")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/variant/include")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/nano-signal-slot")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/brigand")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/hopscotch-map/src")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/readerwriterqueue")
include_directories(SYSTEM "${OSSIA_3RDPARTY_FOLDER}/exprtk")
include_directories(SYSTEM "${3RDPARTY_FOLDER}/miniz")
add_subdirectory("${3RDPARTY_FOLDER}/Qt-Color-Widgets" )
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator" )
add_subdirectory("${3RDPARTY_FOLDER}/QRecentFilesMenu" )

set(SCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins") # todo libraries, components ?
set(SCORE_GLOBAL_COMPONENTS_LIST CACHE INTERNAL "List of components") # todo libraries, components ?

add_subdirectory(base)
set(SCORE_TESTBED OFF)
if(SCORE_TESTBED)
add_subdirectory(Tests/testbed)
endif()

add_custom_target(Docs SOURCES Documentation/Models/score.qmodel)
#if(INTEGRATION_TESTING)

 #       include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Tests")
 #       file(GLOB_RECURSE TESTS_HDRS
 #               "${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.hpp")
  #      add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
  #  setup_score_tests(Tests/Integration)
#endif()

#include(CTest)
