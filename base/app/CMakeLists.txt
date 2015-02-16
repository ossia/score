cmake_minimum_required(VERSION 3.0)

project(iscore_app)

find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Network Xml)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/)

SET(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH ".")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")
add_definitions(-DQT_STATICPLUGIN)
add_executable(iscore_app WIN32 main.cpp)

target_link_libraries(iscore_app iscore_lib)
if(ISCORE_STATIC_PLUGINS)
	add_definitions(-DISCORE_STATIC_PLUGINS)
	target_link_libraries(iscore_app 
		inspector_plugin 
		networkplugin 
		scenario_process 
		pluginsettings_plugin 
		device_explorer_plugin)
endif()
