cmake_minimum_required(VERSION 3.0)

# TODO same for install
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

if(ISCORE_STATIC_PLUGINS)
  add_definitions(-DQT_STATICPLUGIN)
endif()

# TODO use CMake interfaces instead to provide ProcessInterface and InspectorInterface cleanly.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/inspector/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/scenario/")

add_subdirectory(state)

add_subdirectory(plugin_settings)
add_subdirectory(inspector)
add_subdirectory(scenario)
add_subdirectory(network)
add_subdirectory(device_explorer)
add_subdirectory(curve_plugin)
add_subdirectory(iscore_cohesion)
add_subdirectory(ossia)
