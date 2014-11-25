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
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")

file(GLOB_RECURSE DOCUMENT_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/Document/*.hpp")
file(GLOB_RECURSE DOCUMENT_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/Document/*.cpp")
file(GLOB_RECURSE COMMANDS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/Commands/*.hpp")
file(GLOB_RECURSE COMMANDS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/Commands/*.cpp")

add_library(scenario_process SHARED 
			${DOCUMENT_HDRS} ${DOCUMENT_SRCS} ${COMMANDS_HDRS} ${COMMANDS_SRCS})
install(TARGETS scenario_process LIBRARY DESTINATION ${ISCORE_PLUGINS_FOLDER})

target_link_libraries(scenario_process Qt5::Core Qt5::Widgets iscore_lib)
if(APPLE)
	 SET_TARGET_PROPERTIES(scenario_process PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

add_subdirectory(tests)
