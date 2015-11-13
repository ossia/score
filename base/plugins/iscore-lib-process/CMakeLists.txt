cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario)
iscore_common_setup()

option(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS "Enable to have debug rects around elements of a scenario" OFF)
if(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS)
    add_definitions(-DISCORE_SCENARIO_DEBUG_RECTS)
endif()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

### Library ###
file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/Process/*.hpp")
set(PROCESS_SRCS
# TODO move these files bak in Process.
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadataSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNodeSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Focus/FocusDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ProcessFonts.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process
    Qt5::Core Qt5::Widgets iscore_lib_base iscore_lib_state)

setup_iscore_library(iscore_lib_process)
