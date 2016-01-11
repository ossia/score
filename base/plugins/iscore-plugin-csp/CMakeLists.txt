cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_csp LANGUAGES CXX)

iscore_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core)

# Files & main target
include_directories("${3RDPARTY_FOLDER}kiwi")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
                        Qt5::Core
                        iscore_lib_base iscore_lib_process iscore_plugin_scenario)

setup_iscore_plugin(${PROJECT_NAME})
