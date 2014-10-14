cmake_minimum_required(VERSION 3.0)
project(iscore_core)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

# Configuration for the subprojects
set(ISCORE_PLUGINS_FOLDER "${CMAKE_BINARY_DIR}/plugins")

# Packages
find_package(Qt5Core)

# Files & main target
include_directories(.)

set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/library/LibraryElement.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/menu/MenuCommand.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/panels/Panel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/panels/BasePanel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/processes/Process.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface/settings/SettingsGroup.h

    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/PanelFactoryPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/LibraryElementFactoryPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/MenuCommandFactoryPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/ProcessFactoryPluginInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/plugin_interface/SettingsFactoryPluginInterface.h

    ${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.h
)

set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/plugin/PluginManager.cpp
)

add_executable(iscore_core ${SOURCE_FILES} ${HEADER_FILES})

target_link_libraries(iscore_core Qt5::Core)

# Other targets
add_subdirectory(plugins)
add_subdirectory(plugin_interface)
add_subdirectory(interface)
