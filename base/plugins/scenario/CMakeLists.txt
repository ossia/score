cmake_minimum_required(VERSION 3.0)
project(simple_process)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterface" PARENT_SCOPE)

# Packages
find_package(Qt5Core 5.2 REQUIRED)
find_package(Qt5Widgets 5.2 REQUIRED)
find_package(Qt5Xml 5.2 REQUIRED)

# Files & main target
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${ISCORE_INTERFACE_FOLDER}")
include_directories("${ISCORE_INTERFACE_FOLDER}/..")
include_directories("${ISCORE_INTERFACE_FOLDER}/../../../")
include_directories("${ISCORE_INTERFACE_FOLDER}/../../../API/Headers")


file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.hpp")
file(GLOB_RECURSE PROCESS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.cpp")

### TODO Do the same for inspector.
add_library(process_interface ${PROCESS_HDRS} ${PROCESS_SRCS})
target_link_libraries(process_interface Qt5::Core Qt5::Widgets iscore_lib)

add_library(scenario_process ${SRCS} ${HDRS})
target_link_libraries(scenario_process Qt5::Core Qt5::Widgets Qt5::Xml iscore_lib APIMock inspector_plugin process_interface)
if(APPLE)
	 SET_TARGET_PROPERTIES(scenario_process PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
endif()

add_subdirectory(tests)
