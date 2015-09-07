cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_library)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
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

if(ISCORE_COTIRE)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(iscore_plugin_library PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
    cotire(iscore_plugin_library)
endif()
install(TARGETS iscore_plugin_library
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)
