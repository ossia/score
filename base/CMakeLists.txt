cmake_minimum_required(VERSION 2.6)
project(iscore_core)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

include_directories(.)
include_directories(plugins_interface)
include_directories(plugins_interface/capabilities)
include_directories(plugins_interface/capabilities/process)
add_executable(iscore_core main.cpp)

install(TARGETS iscore_core RUNTIME DESTINATION bin)


