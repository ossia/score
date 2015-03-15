cmake_minimum_required(VERSION 3.0)
project(inspector_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)


file(GLOB_RECURSE INTERFACE_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.cpp")

add_library(InspectorLibrary ${INTERFACE_SRCS})
target_link_libraries(InspectorLibrary Qt5::Core Qt5::Widgets iscore_lib)

set_property(TARGET InspectorLibrary APPEND
			 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

# Files & main target
file(GLOB_RECURSE PLUGIN_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.cpp")

add_library(inspector_plugin ${PLUGIN_SRCS})

target_link_libraries(inspector_plugin Qt5::Core Qt5::Widgets iscore_lib InspectorLibrary)

install(TARGETS InspectorLibrary inspector_plugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
