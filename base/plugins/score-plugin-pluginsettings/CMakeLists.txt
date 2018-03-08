cmake_minimum_required(VERSION 3.0)
project(score_plugin_pluginsettings LANGUAGES CXX)

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
set(HEADER_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/commands/BlacklistCommand.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettings.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsPresenter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsView.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginItemModel.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/FileDownloader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_pluginsettings.hpp"
)

set(SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/commands/BlacklistCommand.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsPresenter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginSettingsView.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/PluginItemModel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PluginSettings/FileDownloader.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_pluginsettings.cpp"
)

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

score_moc_headers(${PROJECT_NAME} "${HEADER_FILES}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                        Qt5::Core Qt5::Widgets
                        score_lib_base)

setup_score_plugin(${PROJECT_NAME})
