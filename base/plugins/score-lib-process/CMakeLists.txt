cmake_minimum_required(VERSION 3.0)
project(score_plugin_scenario LANGUAGES CXX)
score_common_setup()

# Packages
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets)

### Library ###
set(PROCESS_HDRS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExpandMode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Focus/FocusDispatcher.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelPanelProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/GenericProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessComponent.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMetadata.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Port.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Cable.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMimeSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ToolPalette.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ZoomHelper.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Actions/ProcessActions.hpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerPanelProxy.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectFactory.hpp"
)
set(PROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNodeSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerModelPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Port.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Cable.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.cpp"
)

add_library(score_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS}  )
target_link_libraries(score_lib_process  PUBLIC
    Qt5::Core Qt5::Widgets score_lib_base score_lib_state score_lib_inspector)

target_compile_definitions(score_lib_process PUBLIC
    $<$<BOOL:${DEFINE_SCORE_SCENARIO_DEBUG_RECTS}>:SCORE_SCENARIO_DEBUG_RECTS>)

setup_score_library(score_lib_process)

generate_export_header(score_lib_process)

setup_score_tests(Tests)
