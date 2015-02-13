cmake_minimum_required(VERSION 3.0)
project(inspector_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets)

# Files & main target
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")
include_directories("${ISCORE_INTERFACE_FOLDER}/../../../")
# TODO mieux
if(APPLE)
        include_directories(/usr/local/Cellar/boost/1.56.0/include)
endif()

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(inspector_plugin ${HDRS} ${SRCS} ${UIS})

target_link_libraries(inspector_plugin Qt5::Core Qt5::Widgets iscore_lib)
if(APPLE)
	 SET_TARGET_PROPERTIES(inspector_plugin PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
