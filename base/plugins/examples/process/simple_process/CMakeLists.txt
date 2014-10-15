cmake_minimum_required(VERSION 3.0)
project(simple_process)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

# Packages
find_package(Qt5Core)

# Files & main target
include_directories(.)

set(HEADER_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.hpp
)

set(SOURCE_FILES
	${CMAKE_CURRENT_SOURCE_DIR}/SimpleProcessPlugin.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/HelloWorldProcess.cpp
)

add_library(simple_process SHARED ${HEADER_FILES} ${SOURCE_FILES})
install(TARGETS simple_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(simple_process Qt5::Core)
