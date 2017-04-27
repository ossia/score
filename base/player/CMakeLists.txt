cmake_minimum_required(VERSION 3.0)
project(iscore_player LANGUAGES CXX)

iscore_common_setup()
enable_testing()

find_package(Qt5 5.7 COMPONENTS QuickControls2)

add_executable(player "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

target_link_libraries(player PUBLIC iscore_lib_base iscore_plugin_scenario iscore_plugin_engine Qt5::QuickControls2)

if(ISCORE_STATIC_PLUGINS)
  iscore_write_static_plugins_header()
  target_link_libraries(player PUBLIC ${ISCORE_PLUGINS_LIST})
endif()

setup_iscore_common_exe_features(player)
