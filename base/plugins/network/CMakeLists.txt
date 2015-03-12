cmake_minimum_required(VERSION 3.0)
project(network_plugin)

if(ANDROID OR WIN32)
	message("TODO FIXME ON ANDROID AND WINDOWS")
	return()
endif()
add_subdirectory(Deps)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Network)

set(CMAKE_AUTOMOC ON)

# Files & main target
include_directories(.)

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour-qt/headers")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Deps/oscpack")

file(GLOB_RECURSE SRCS

	"${CMAKE_CURRENT_SOURCE_DIR}/Deps/bonjour*.*"
	"${CMAKE_CURRENT_SOURCE_DIR}/remote/*.*"
	"${CMAKE_CURRENT_SOURCE_DIR}/Repartition/*.*"
	"${CMAKE_CURRENT_SOURCE_DIR}/Serialization/*.*"
	"${CMAKE_CURRENT_SOURCE_DIR}/settings_impl/*.*"
	"${CMAKE_CURRENT_SOURCE_DIR}/zeroconf/*.*")

add_library(networkplugin ${SRCS} NetworkCommand.cpp NetworkCommand.hpp NetworkDocumentPlugin.cpp NetworkDocumentPlugin.hpp NetworkPlugin.cpp NetworkPlugin.hpp)

target_link_libraries(networkplugin
									iscore_lib
									Qt5::Core
									Qt5::Widgets
									Qt5::Network
									pthread
									oscpack)

if(NOT APPLE)
	target_link_libraries(networkplugin dns_sd)
endif()

install(TARGETS networkplugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
