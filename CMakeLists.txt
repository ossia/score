project(iscore)
cmake_minimum_required(VERSION 3.1)


set(APPNAME "i-score")
set(ISCORE_VERSION_MAJOR 1)
set(ISCORE_VERSION_MINOR 0)
set(ISCORE_VERSION_PATCH 0)
set(ISCORE_VERSION_EXTRA a4)
set(ISCORE_CODENAME "Juste Leblanc")
set(ISCORE_VERSION "${ISCORE_VERSION_MAJOR}.${ISCORE_VERSION_MINOR}.${ISCORE_VERSION_PATCH}-${ISCORE_VERSION_EXTRA}")

enable_testing()
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreConfiguration.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/CMake/IScoreFunctions.cmake")

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/API")

# too hardcore for jamoma
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
#         add_definitions(-D_GLIBCXX_DEBUG)
#     endif()
# endif()

if(ANDROID)
  set(ISCORE_STATIC_PLUGINS True)
  set(Boost_FOUND True)
  include_directories("/opt/android-toolchain/arm-linux-androideabi/include")
else()
  find_package(Boost REQUIRED)
  include_directories("${Boost_INCLUDE_DIRS}")
endif()

if(APPLE AND DEPLOYMENT_BUILD)
  set(ISCORE_STATIC_PLUGINS True)
  add_definitions(-DISCORE_DEPLOYMENT_BUILD)
endif()

if(INTEGRATION_TESTING)
  set(ISCORE_STATIC_PLUGINS True)
endif()

if(ISCORE_STATIC_PLUGINS)
    set(BUILD_SHARED_LIBS OFF)
    add_definitions(-DQT_STATICPLUGIN)
else()
    set(BUILD_SHARED_LIBS ON)
endif()

find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets Gui Network Xml PrintSupport Svg Test LinguistTools DBus)

set(3RDPARTY_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/")
set(ISCORE_ROOT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${ISCORE_ROOT_BINARY_DIR}")
include_directories("${3RDPARTY_FOLDER}/variant/include")
add_subdirectory("${3RDPARTY_FOLDER}/QProgressIndicator")

set(ISCORE_PLUGINS_LIST CACHE INTERNAL "List of plugins")

add_subdirectory(base)

if(INTEGRATION_TESTING)

	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Tests")
	file(GLOB_RECURSE TESTS_HDRS
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.hpp")
	add_custom_target(MocksHeaders SOURCES ${TESTS_HDRS})
    setup_iscore_tests(Tests/Integration)
endif()

include(CTest)
