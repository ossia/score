cmake_minimum_required(VERSION 3.0)

project(iscore_base)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")
set(ISCORE_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/lib/interface")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/core")
add_subdirectory(lib)
add_subdirectory(plugins)
if(ANDROID)
	add_subdirectory(app-android)
else()
	add_subdirectory(app)
endif()
