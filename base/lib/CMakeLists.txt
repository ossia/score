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
include_directories(core)
include_directories("${CMAKE_SOURCE_DIR}")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/core/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/core/*.cpp")
file(GLOB_RECURSE INTERFACE_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/interface/*.hpp")
file(GLOB_RECURSE INTERFACE_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/interface/*.cpp")

add_library(iscore_lib ${HDRS} ${SRCS} ${INTERFACE_HDRS} ${INTERFACE_SRCS})
target_link_libraries(iscore_lib Qt5::Core Qt5::Widgets)

install(TARGETS iscore_lib
		LIBRARY DESTINATION .
		ARCHIVE DESTINATION .)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
