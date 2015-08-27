cmake_minimum_required(VERSION 3.0)

# TODO same for install
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/)

# TODO use CMake interfaces instead to provide ProcessInterface and InspectorInterface cleanly.
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/iscore-plugin-inspector/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/iscore-plugin-scenario/")

add_subdirectory(iscore-lib-state)

add_subdirectory(iscore-plugin-pluginsettings)
add_subdirectory(iscore-plugin-inspector)
add_subdirectory(iscore-plugin-library)
add_subdirectory(iscore-plugin-deviceexplorer)
add_subdirectory(iscore-plugin-scenario)
add_subdirectory(iscore-plugin-curve)
add_subdirectory(iscore-plugin-ossia)
add_subdirectory(iscore-plugin-cohesion)

if(ISCORE_USE_DEV_PLUGINS)
add_subdirectory(iscore-plugin-network)
add_subdirectory(iscore-plugin-space)
endif()
