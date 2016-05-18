cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_inspector LANGUAGES CXX)

iscore_common_setup()

# Inspector panel plug-in
file(GLOB_RECURSE PLUGIN_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/*.hpp")
set(PLUGIN_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/iscore_plugin_inspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/SelectionStackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelDelegate.cpp"
)

add_library(${PROJECT_NAME} ${PLUGIN_HDRS} ${PLUGIN_SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_lib_inspector)

setup_iscore_plugin(iscore_plugin_inspector)
