cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_pluginsettings LANGUAGES CXX)

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/commands/BlacklistCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettingsPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsView.hpp"
)

set(SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/commands/BlacklistCommand.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettingsPlugin.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettings.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsView.cpp"
)

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base)

setup_iscore_plugin(${PROJECT_NAME})
