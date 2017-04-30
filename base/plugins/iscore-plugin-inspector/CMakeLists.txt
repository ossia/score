cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_inspector LANGUAGES CXX)

iscore_common_setup()

# Inspector panel plug-in
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/iscore_plugin_inspector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/SelectionStackWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelDelegate.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.hpp"
  )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/iscore_plugin_inspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/SelectionStackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelDelegate.cpp"
)

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_lib_inspector)

setup_iscore_plugin(iscore_plugin_inspector)
