cmake_minimum_required(VERSION 3.0)
project(iscore_core)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")

# Packages
find_package(Qt5Core 5.2 REQUIRED)
find_package(Qt5Widgets 5.2 REQUIRED)

# Files & main target
include_directories(.)
include_directories(core)
include_directories("${CMAKE_SOURCE_DIR}")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(iscore_lib ${HDRS} ${SRCS})
target_link_libraries(iscore_lib Qt5::Core Qt5::Widgets)

install(TARGETS iscore_lib DESTINATION ${CMAKE_BINARY_DIR})
