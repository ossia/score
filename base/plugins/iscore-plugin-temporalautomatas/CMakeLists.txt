cmake_minimum_required(VERSION 3.1)
project(iscore_plugin_temporalautomatas LANGUAGES CXX)

iscore_common_setup()

# Files & main target
set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioMetrics.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioVisitor.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ScenarioGenerator.hpp"
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
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_lib_base iscore_lib_process iscore_plugin_scenario iscore_plugin_automation)

setup_iscore_plugin(${PROJECT_NAME})
