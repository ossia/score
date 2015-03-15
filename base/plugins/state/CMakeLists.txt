cmake_minimum_required(VERSION 2.8)
project(StateLibrary)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/State/*.cpp")

add_library(StateLibrary ${SRCS} ${HDRS})
target_link_libraries(StateLibrary Qt5::Core Qt5::Widgets)


set_property(TARGET StateLibrary APPEND
			 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")


install(TARGETS StateLibrary
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
