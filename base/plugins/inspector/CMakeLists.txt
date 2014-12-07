cmake_minimum_required(VERSION 2.8)
project(inspector_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")
include_directories("${ISCORE_INTERFACE_FOLDER}/../../../")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB_RECURSE UIS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui")

add_library(inspector_plugin SHARED	${HDRS} ${SRCS} ${UIS})

target_link_libraries(inspector_plugin Qt5::Core Qt5::Widgets iscore_lib)
if(APPLE)
	 SET_TARGET_PROPERTIES(inspector_plugin PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
