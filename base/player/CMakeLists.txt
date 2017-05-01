cmake_minimum_required(VERSION 3.0)
project(iscore_player LANGUAGES CXX)

iscore_common_setup()
enable_testing()
set(CMAKE_AUTOMOC ON)
find_package(Qt5 5.7 COMPONENTS QuickControls2)

add_library(iscore_player SHARED
  "${CMAKE_CURRENT_SOURCE_DIR}/player_impl.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/player.cpp")
target_link_libraries(iscore_player PUBLIC
  iscore_lib_base iscore_plugin_scenario iscore_plugin_engine
  iscore_plugin_pluginsettings iscore_plugin_inspector iscore_plugin_js
  iscore_plugin_midi iscore_plugin_recording
  Qt5::QuickControls2)

add_executable(player "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(player PUBLIC iscore_player)

add_executable(player_device "${CMAKE_CURRENT_SOURCE_DIR}/device_example.cpp")
target_link_libraries(player_device PUBLIC iscore_player)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(player PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_lib_features(iscore_player)
setup_iscore_common_exe_features(player)
setup_iscore_common_exe_features(player_device)
