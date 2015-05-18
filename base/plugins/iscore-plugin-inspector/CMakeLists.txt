cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_inspector)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets)

# Inspector interface
file(GLOB_RECURSE INTERFACE_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Inspector/*.cpp")

add_library(iscore_lib_inspector ${INTERFACE_SRCS})
target_link_libraries(iscore_lib_inspector Qt5::Core Qt5::Widgets iscore_lib_base)

set_property(TARGET iscore_lib_inspector APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")

install(TARGETS iscore_lib_inspector
        LIBRARY DESTINATION .
        ARCHIVE DESTINATION static_lib/)

# Inspector panel plug-in
file(GLOB_RECURSE PLUGIN_SRCS
        "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Plugin/*.cpp")

add_library(iscore_plugin_inspector ${PLUGIN_SRCS})

target_link_libraries(iscore_plugin_inspector iscore_lib_inspector)
if(USE_COTIRE)
        cotire(iscore_plugin_inspector)
endif()
install(TARGETS iscore_plugin_inspector
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
