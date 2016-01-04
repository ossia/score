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
set(PROCESS_HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExpandMode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Focus/FocusDispatcher.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelPanelProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadata.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactoryKey.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessFactoryKey.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcess.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ProcessFonts.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ToolPalette.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ZoomHelper.hpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetFactory.hpp"
)
set(PROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadataSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNodeSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Focus/FocusDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcess.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ProcessFonts.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetFactory.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process
    Qt5::Core Qt5::Widgets iscore_lib_base iscore_lib_state)
setup_iscore_library(iscore_lib_process)

generate_export_header(iscore_lib_process)
