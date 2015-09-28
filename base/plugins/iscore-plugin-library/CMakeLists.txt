cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_library)

iscore_common_setup()

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets)

file(GLOB_RECURSE PLUGIN_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.cpp")

add_library(iscore_plugin_library ${PLUGIN_SRCS})

target_link_libraries(
  iscore_plugin_library
  Qt5::Core Qt5::Widgets
  iscore_lib_base iscore_lib_state iscore_plugin_deviceexplorer)

setup_iscore_plugin(iscore_plugin_library)
