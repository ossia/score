cmake_minimum_required(VERSION 3.0)
project(device_explorer_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Test Xml)

# Files & main target

### Library ###
file(GLOB_RECURSE INTERFACE_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.hpp")

#add_library(DeviceExplorerLibrary ${INTERFACE_SRCS})

#set_property(TARGET DeviceExplorerLibrary APPEND
#       PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
#target_link_libraries(DeviceExplorerLibrary Qt5::Core Qt5::Widgets  Qt5::Xml iscore_lib  StateLibrary)

#install(TARGETS DeviceExplorerLibrary
#    LIBRARY DESTINATION .
#    ARCHIVE DESTINATION static_lib/)

### Singletons ###

#add_library(DeviceExplorerSingleton
#    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/SingletonProtocolList.hpp"
#    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/SingletonProtocolList.cpp"
#    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/NodeSerialization.cpp"
#    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/DeviceExplorerInterface.cpp")

#target_link_libraries(DeviceExplorerSingleton DeviceExplorerLibrary)

#install(TARGETS DeviceExplorerSingleton
#    LIBRARY DESTINATION .
#    ARCHIVE DESTINATION static_lib/)

### Plugin ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Plugin")
file(GLOB_RECURSE PLUGIN_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.*pp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Singletons/*.*pp"
    "${CMAKE_CURRENT_SOURCE_DIR}/DeviceExplorer/*.*pp")

qt5_add_resources(EXPLORER_RESOURCES_RCC "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/DeviceExplorer.qrc")

add_library(device_explorer_plugin
      ${PLUGIN_SRCS}
      ${EXPLORER_RESOURCES_RCC})

target_link_libraries(device_explorer_plugin Qt5::Core Qt5::Widgets  Qt5::Xml iscore_lib  StateLibrary Qt5::Test)

set_property(TARGET device_explorer_plugin APPEND
       PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS device_explorer_plugin
    LIBRARY DESTINATION plugins/
    ARCHIVE DESTINATION static_plugins/)
