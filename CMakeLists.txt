project(iscore)
cmake_minimum_required(VERSION 3.0)

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/modules")
set(CMAKE_ANDROID_PATH "${CMAKE_SOURCE_DIR}/CMake/Android")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas")

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/API")
	add_subdirectory("${CMAKE_SOURCE_DIR}/API")
endif()

add_subdirectory(base)
add_subdirectory(tests)
