cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_ossia)

iscore_common_setup()

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets Xml PrintSupport Test)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build OSSIA plugin")
    return()
endif()

# Files & main target
# TODO no glob-recurse
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_deviceexplorer
                        OSSIAHeadersLib APIJamoma)

setup_iscore_plugin(${PROJECT_NAME})
