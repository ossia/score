cmake_minimum_required(VERSION 3.0)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

if(ISCORE_STATIC_PLUGINS)
	add_definitions(-DQT_STATICPLUGIN)
endif()

# TODO use CMake interfaces instead to provide ProcessInterface and InspectorInterface cleanly.
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inspector")
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/inspector/")
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scenario")
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/scenario/")
endif()


# add_subdirectory(examples)

add_subdirectory(plugin_settings)

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/inspector")
	add_subdirectory(inspector)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/scenario")
	add_subdirectory(scenario)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/network")
	add_subdirectory(network)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/device_explorer")
	add_subdirectory(device_explorer)
endif()

if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/curve_plugin")
#	add_subdirectory(curve_plugin)
endif()
