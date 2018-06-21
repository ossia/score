cmake_minimum_required(VERSION 3.0)
project(score_plugin_inspector LANGUAGES CXX)

score_common_setup()

# Inspector panel plug-in
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_inspector.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelDelegate.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.hpp"
  )
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_inspector.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/Implementation/InspectorPanel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/InspectorPlugin/Panel/InspectorPanelDelegate.cpp"
)

add_library(${PROJECT_NAME} ${HDRS} ${SRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC score_lib_inspector)

setup_score_plugin(score_plugin_inspector)
