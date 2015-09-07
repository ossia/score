cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_ossia)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
cmake_policy(SET CMP0042 NEW)

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets Xml PrintSupport Test)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build OSSIA plugin")
    return()
endif()

# For OSSIA API
include_directories(${ISCORE_ROOT_SOURCE_DIR})
include_directories(${ISCORE_ROOT_SOURCE_DIR}/API)
include_directories(${ISCORE_ROOT_SOURCE_DIR}/API/Headers)
# Files & main target
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${SCENARIO_INTERFACE_FOLDER}")
include_directories("${CURVE_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
# TODO put this in CMake interface of iscore_lib

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_deviceexplorer APIJamoma)

iscore_cotire(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/
        COMPONENT DynamicRuntime)
