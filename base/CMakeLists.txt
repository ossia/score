cmake_minimum_required(VERSION 3.0)

project(iscore_base)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")
include_directories(lib)

add_subdirectory(lib)
add_subdirectory(plugins)
if(ANDROID)
	add_subdirectory(app-android)
else()
	add_subdirectory(app)
endif()
