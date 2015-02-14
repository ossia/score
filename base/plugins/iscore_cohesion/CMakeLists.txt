cmake_minimum_required(VERSION 2.8)
project(IScoreCohesion)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Xml)

find_package(Jamoma 0.6)
if(NOT ${Jamoma_FOUND})
    message("Jamoma not found - won't build iscore cohesion plugin")
    return()
endif()


# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../scenario/source")
# TODO put this in CMake interface of iscore_lib
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")
include_directories("${ISCORE_INTERFACE_FOLDER}/../../../")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib scenario_process CurvePlugin device_explorer_plugin
                        Jamoma::Foundation Jamoma::Modular Jamoma::Score )

