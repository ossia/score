cmake_minimum_required(VERSION 2.8)
project(simple_process)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5Core 5.3 REQUIRED)
find_package(Qt5Widgets 5.3 REQUIRED)

# Files & main target
include_directories(.)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/Document")


file(GLOB_RECURSE HDRS *.hpp)
file(GLOB_RECURSE SRCS *.cpp)

add_library(scenario_process SHARED ${HDRS} ${SRCS})
install(TARGETS scenario_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(scenario_process Qt5::Core Qt5::Widgets)
if(APPLE)
	 SET_TARGET_PROPERTIES(scenario_process PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()
