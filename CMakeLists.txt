project(iscore)
cmake_minimum_required(VERSION 3.0)


# Note : if building with a Qt installed in e.g. /home/myuser/Qt/ or /Users/Qt or c:\Qt\
# keep in mind that you have to call CMake with :
# $ cmake -DCMAKE_MODULE_PATH={path/to/qt/5.3}/{gcc64,clang,msvc2013...}/lib/cmake/Qt5

# Settings
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/modules")
set(CMAKE_ANDROID_PATH "${CMAKE_SOURCE_DIR}/CMake/Android")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pipe -Wall -Wextra -Wno-unused-parameter -Wno-unknown-pragmas")

if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/API")
	add_subdirectory("${CMAKE_SOURCE_DIR}/API")
endif()

if(ISCORE_STATIC_PLUGINS OR ANDROID)
	set(BUILD_SHARED_LIBS OFF)
else()
	set(BUILD_SHARED_LIBS ON)
endif()

add_subdirectory(base)
add_subdirectory(tests)
