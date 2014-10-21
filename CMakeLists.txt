project(iscore)
cmake_minimum_required(VERSION 2.8)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/modules")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 ")

add_subdirectory(base)
add_subdirectory(tests)

if(IS_DIRECTORY API)
	add_subdirectory(API)
endif()
