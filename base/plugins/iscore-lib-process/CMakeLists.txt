cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario LANGUAGES CXX)
iscore_common_setup()

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
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMetadata.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessFactoryList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcess.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ProcessFonts.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/Skin.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ColorReference.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ToolPalette.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ZoomHelper.hpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.hpp"
)
set(PROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ModelMetadataSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/StateProcessSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNodeSerialization.cpp"

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
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/Skin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ColorReference.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process  PUBLIC
    Qt5::Core Qt5::Widgets iscore_lib_base iscore_lib_state)

target_compile_definitions(iscore_lib_process PUBLIC
    $<$<BOOL:${DEFINE_ISCORE_SCENARIO_DEBUG_RECTS}>:ISCORE_SCENARIO_DEBUG_RECTS>)

setup_iscore_library(iscore_lib_process)

generate_export_header(iscore_lib_process)
