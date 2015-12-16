cmake_minimum_required(VERSION 2.8)
project(iscore_plugin_temporalautomatas)

iscore_common_setup()

# Files & main target
set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioMetrics.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioVisitor.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioGenerator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/TAConversion.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_temporalautomatas.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioMetrics.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioGenerator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/TAConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_temporalautomatas.cpp"
)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/TemporalAutomatas/Resources/TAResources.qrc")
add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(${PROJECT_NAME} iscore_plugin_scenario)

setup_iscore_plugin(${PROJECT_NAME})
