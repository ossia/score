cmake_minimum_required(VERSION 3.0)
project(scenario_plugin)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterface" PARENT_SCOPE)

# Packages
find_package(Qt5 5.2 REQUIRED COMPONENTS Core Widgets Xml)

### Library ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../device_explorer")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../") # API
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/Headers/")

file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.hpp")
file(GLOB_RECURSE PROCESS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.cpp")

add_library(ProcessLibrary ${PROCESS_HDRS} ${PROCESS_SRCS})
target_link_libraries(ProcessLibrary Qt5::Core Qt5::Widgets iscore_lib)
install(TARGETS ProcessLibrary
		LIBRARY DESTINATION ./
		ARCHIVE DESTINATION static_lib/)


### Plugin ###
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

add_library(scenario_process ${SRCS} ${HDRS})
target_link_libraries(scenario_process Qt5::Core Qt5::Widgets Qt5::Xml
									   iscore_lib
									   InspectorLibrary ProcessLibrary DeviceExplorerLibrary  StateLibrary)


install(TARGETS scenario_process
		LIBRARY DESTINATION plugins/
		ARCHIVE DESTINATION static_plugins/)

add_subdirectory(tests)
