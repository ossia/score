cmake_minimum_required(VERSION 3.0)
project(scenario_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterface" PARENT_SCOPE)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Xml)

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
target_link_libraries(scenario_process Qt5::Core Qt5::Widgets Qt5::Xml
                                       iscore_lib APIMock
                                       inspector_plugin process_interface device_explorer_plugin)

install(TARGETS process_interface
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)
install(TARGETS scenario_process
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)

add_subdirectory(tests)
