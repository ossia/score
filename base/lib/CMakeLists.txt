cmake_minimum_required(VERSION 3.0)
project(iscore_core)

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets )

if(NOT Boost_FOUND)
		message("The network plugin requires boost to build")
		return()
endif()

# Files & main target
include_directories(.)

file(GLOB_RECURSE SRCS
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/*.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/public_interface/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/public_interface/*.hpp)

add_library(iscore_lib ${SRCS})
target_link_libraries(iscore_lib Qt5::Core Qt5::Widgets)

install(TARGETS iscore_lib
		LIBRARY DESTINATION .
		ARCHIVE DESTINATION .)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
