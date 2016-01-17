cmake_minimum_required(VERSION 3.1)
project(iscore_addon_staticanalysis LANGUAGES CXX)

iscore_common_setup()

# Files & main target
set(HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioMetrics.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioVisitor.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioGenerator.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/TAConversion.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_staticanalysis.hpp"
)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioMetrics.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioVisitor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/ScenarioGenerator.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/TAConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/iscore_plugin_staticanalysis.cpp"
)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/StaticAnalysis/Resources/TAResources.qrc")
add_library(${PROJECT_NAME} ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(${PROJECT_NAME} PUBLIC iscore_lib_base iscore_lib_process iscore_plugin_scenario iscore_plugin_automation)

setup_iscore_plugin(${PROJECT_NAME})
