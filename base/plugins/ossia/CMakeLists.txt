cmake_minimum_required(VERSION 2.8)
project(iscore-plugin-ossia)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
cmake_policy(SET CMP0042 NEW)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Xml)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build OSSIA plugin")
    return()
endif()

# For OSSIA API
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/API)
include_directories(${CMAKE_SOURCE_DIR}/API/Headers)
# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../scenario/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../curve_plugin")
# TODO put this in CMake interface of iscore_lib

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib scenario_process CurvePlugin device_explorer_plugin)

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
