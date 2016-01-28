cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_cohesion LANGUAGES CXX)

iscore_common_setup()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

# Files & main target
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/IScoreCohesionCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Actions/CreateCurves.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Actions/SnapshotParameters.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Record/RecordManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_cohesion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/IScoreCohesionApplicationPlugin.cpp"
)
add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
iscore_generate_command_list_file(${PROJECT_NAME} "${HDRS}")
target_link_libraries(${PROJECT_NAME} PUBLIC
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_lib_process iscore_plugin_scenario iscore_plugin_curve iscore_plugin_automation iscore_lib_device iscore_plugin_deviceexplorer)

setup_iscore_plugin(${PROJECT_NAME})
