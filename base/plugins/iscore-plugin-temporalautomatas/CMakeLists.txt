cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_temporalautomatas)

iscore_common_setup()

# Files & main target
set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioVisitor.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_temporalautomatas.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_temporalautomatas.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})
target_link_libraries(${PROJECT_NAME} iscore_plugin_scenario)

setup_iscore_plugin(${PROJECT_NAME})
