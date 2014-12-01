cmake_minimum_required(VERSION 3.0)

project(iscore_app)

find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)
find_package(Qt5OpenGL 5.3 REQUIRED)

SET(CMAKE_SKIP_BUILD_RPATH FALSE)
SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
SET(CMAKE_INSTALL_RPATH ".")
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/core")

add_executable(iscore_app main.cpp)

target_link_libraries(iscore_app iscore_lib)

install(TARGETS iscore_app DESTINATION ${CMAKE_BINARY_DIR})

