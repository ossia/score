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
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/GenericProcessFactory.hpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMimeSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ToolPalette.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ZoomHelper.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Actions/ProcessActions.hpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactoryList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyState.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerModel.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerPanelProxy.hpp"
)
set(PROCESS_SRCS
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
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ProcessFonts.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyLayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyLayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/DummyModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerModelSerialization.cpp"
)

qt5_add_resources(DUMMYPROCESS_QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Process/Dummy/resources/DummyResources.qrc")

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS}  ${DUMMYPROCESS_QRCS} )
target_link_libraries(iscore_lib_process  PUBLIC
    Qt5::Core Qt5::Widgets iscore_lib_base iscore_lib_state iscore_lib_inspector)

target_compile_definitions(iscore_lib_process PUBLIC
    $<$<BOOL:${DEFINE_ISCORE_SCENARIO_DEBUG_RECTS}>:ISCORE_SCENARIO_DEBUG_RECTS>)

setup_iscore_library(iscore_lib_process)

generate_export_header(iscore_lib_process)
