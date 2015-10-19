cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_cohesion)

iscore_common_setup()
# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Xml PrintSupport Test)

# Files & main target
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateCurveFromStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Commands/CreateCurvesFromAddresses.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Actions/CreateCurves.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Actions/InterpolateStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Actions/SnapshotParameters.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Record/RecordManager.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_cohesion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/IScoreCohesionControl.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME}
                        Qt5::Core Qt5::Widgets
                        iscore_lib_base iscore_plugin_scenario iscore_plugin_curve iscore_plugin_deviceexplorer)

setup_iscore_plugin(${PROJECT_NAME})
