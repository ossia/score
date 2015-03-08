cmake_minimum_required(VERSION 3.0)
project(inspector_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(inspector_plugin ${HDRS} ${SRCS} ${UIS})

target_link_libraries(inspector_plugin Qt5::Core Qt5::Widgets iscore_lib)

install(TARGETS inspector_plugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
