cmake_minimum_required(VERSION 3.0)
project(network_plugin)

if(ANDROID OR WIN32)
	message("TODO FIXME ON ANDROID AND WINDOWS")
	return()
endif()

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Network)

set(CMAKE_AUTOMOC ON)

# Files & main target
include_directories(.)

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")

file(GLOB_RECURSE SRCS

	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

add_library(networkplugin ${SRCS})

target_link_libraries(networkplugin
									iscore_lib
									Qt5::Core
									Qt5::Widgets
									Qt5::Network
									pthread)

if(NOT APPLE)
	target_link_libraries(networkplugin dns_sd)
endif()

install(TARGETS networkplugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
