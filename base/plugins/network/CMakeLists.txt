cmake_minimum_required(VERSION 3.0)
project(network_plugin)

if(ANDROID OR WIN32)
	message("TODO FIXME ON ANDROID AND WINDOWS")
	return()
endif()

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Network)
find_package(KF5DNSSD NO_MODULE)

set(CMAKE_AUTOMOC ON)

# Files & main target
include_directories(.)

include_directories("${CMAKE_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Repartition")

file(GLOB_RECURSE SRCS
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")


foreach(item ${SRCS})
  if(${item} MATCHES "Zeroconf")
	list(REMOVE_ITEM SRCS ${item})
  endif()
endforeach()

if(${KF5DNSSD_FOUND})
	message("Will use ZeroConf")
	file(GLOB_RECURSE ZEROCONF_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Zeroconf/*.hpp")
endif()

add_library(networkplugin ${SRCS} ${ZEROCONF_SRCS})

target_link_libraries(networkplugin
									iscore_lib
									Qt5::Core
									Qt5::Widgets
									Qt5::Network)

if(${KF5DNSSD_FOUND})
	target_compile_definitions(networkplugin PUBLIC USE_ZEROCONF)
	target_link_libraries(networkplugin KF5::DNSSD)
endif()

if(NOT APPLE)
	target_link_libraries(networkplugin dns_sd)
endif()

install(TARGETS networkplugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
