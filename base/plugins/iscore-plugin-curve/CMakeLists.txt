cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_curve)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

cmake_policy(SET CMP0020 NEW)
cmake_policy(SET CMP0042 NEW)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets PrintSupport Xml Test)

# Files & main target
include_directories("${SCENARIO_PROCESS_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")

file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                     Qt5::Core Qt5::Widgets Qt5::PrintSupport
                     iscore_lib_base iscore_lib_inspector iscore_plugin_deviceexplorer iscore_lib_process iscore_lib_state)

if(USE_COTIRE)
	cotire(${PROJECT_NAME})
endif()

install(TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION plugins/
        ARCHIVE DESTINATION static_plugins/)
