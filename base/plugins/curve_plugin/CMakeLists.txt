cmake_minimum_required(VERSION 2.8)
project(CurvePlugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${SCENARIO_PROCESS_INTERFACE_FOLDER}")

# TODO mieux
if(APPLE)
        include_directories(/usr/local/Cellar/boost/1.56.0/include)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

include_directories(".")
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Widgets iscore_lib inspector_plugin device_explorer_plugin process_interface)

