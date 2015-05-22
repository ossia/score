cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterface" PARENT_SCOPE)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Xml Svg Test)

### Library ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../iscore-plugin-deviceexplorer")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../") # API
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/Headers/")

file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.hpp")
file(GLOB_RECURSE PROCESS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.cpp")

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process Qt5::Core Qt5::Widgets iscore_lib_base)
install(TARGETS iscore_lib_process
    LIBRARY DESTINATION ./
    ARCHIVE DESTINATION static_lib/)


### Plugin ###
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
file(GLOB_RECURSE QRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.qrc")

add_library(iscore_plugin_scenario ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(iscore_plugin_scenario Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Svg
                     iscore_lib_base
                     iscore_lib_inspector iscore_lib_process iscore_plugin_deviceexplorer iscore_lib_state pthread)
if(USE_COTIRE)
        cotire(iscore_plugin_scenario)
endif()

install(TARGETS iscore_plugin_scenario
    LIBRARY DESTINATION plugins/
    ARCHIVE DESTINATION static_plugins/)

#add_subdirectory(tests)
