cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_csp)

iscore_common_setup()

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core)

# Files & main target
include_directories("${3RDPARTY_FOLDER}kiwi")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core
                        iscore_lib_base iscore_plugin_scenario)

setup_iscore_plugin(${PROJECT_NAME})
