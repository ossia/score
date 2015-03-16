cmake_minimum_required(VERSION 3.0)
project(device_explorer_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target


find_package(Qt5Test)
file(GLOB_RECURSE INTERFACE_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.hpp")

add_library(DeviceExplorerLibrary
			${INTERFACE_SRCS})

set_property(TARGET DeviceExplorerLibrary APPEND
			 PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(DeviceExplorerLibrary Qt5::Core Qt5::Widgets iscore_lib  StateLibrary)


include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Plugin")
file(GLOB_RECURSE PLUGIN_SRCS
		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp")

qt5_add_resources(EXPLORER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorer.qrc")

add_library(device_explorer_plugin
			${PLUGIN_SRCS}
			${EXPLORER_RESOURCES_RCC})

target_link_libraries(device_explorer_plugin DeviceExplorerLibrary Qt5::Test)


install(TARGETS device_explorer_plugin
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
