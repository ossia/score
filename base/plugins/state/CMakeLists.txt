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

file(GLOB_RECURSE LIBRARY_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/State/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/State/*.hpp")

add_library(StateLibrary ${LIBRARY_SRCS})
target_link_libraries(StateLibrary Qt5::Core Qt5::Widgets)

set_property(TARGET StateLibrary APPEND
			 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")


#file(GLOB_RECURSE PLUGIN_SRCS
#		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.cpp"
#		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp")
#
#add_library(StatePlugin ${PLUGIN_SRCS})
#target_link_libraries(StatePlugin StateLibrary InspectorLibrary)



install(TARGETS StateLibrary # StatePlugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)

add_subdirectory(Tests)
