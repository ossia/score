project(score_lib_process LANGUAGES CXX)
score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets)

### Library ###
set(PROCESS_HDRS

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/ProcessCommandFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/EditPort.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/SetControlValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/Properties.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ApplicationPlugin.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ControlMessage.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Instantiations.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionFunctions.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExpandMode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Focus/FocusDispatcher.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/HeaderDelegate.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/GenericProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFlags.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessComponent.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMetadata.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessList.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/AudioPortComboBox.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Port.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PrettyPortName.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/TimeSignature.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortItem.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortType.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Cable.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/CableItem.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/CableCopy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/NodeItem.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortListWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/ControlWidgets.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/ControlWidgetDomains.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/WidgetInlets.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/MinMaxFloatPort.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/OfflineAction/OfflineAction.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Drop/ProcessDropHandler.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/Pixmaps.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValueSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TypeConversion.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessMimeSerialization.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ToolPalette.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Preset.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/PresetHelpers.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ZoomHelper.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/LoadPreset.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Magnetism/MagnetismAdjuster.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Magnetism/MagneticInfo.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Actions/ProcessActions.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionContext.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionSetup.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionAction.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionComponent.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetProcessFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerPresenter.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/DocumentPlugin.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Automatable/AutomatableFactory.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Script/ScriptWidget.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Script/ScriptEditor.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Script/MultiScriptEditor.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectFactory.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectLayer.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectPainting.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectLayout.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Execution/ProcessComponent.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Control/Widgets.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Control/Layout.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Control/DefaultEffectItem.hpp"

"${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/ProcessComponent.hpp"
"${CMAKE_CURRENT_SOURCE_DIR}/score_lib_process.hpp"
)
set(PROCESS_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/ProcessCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/EditPort.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/MoveNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Commands/SetControlValue.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Instantiations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNodeSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/ApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ControlMessage.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/HeaderDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/LayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/TimeValue.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/ScenarioStyle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Style/Pixmaps.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/AudioPortComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Port.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/Cable.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/CableItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/ControlWidgets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/NodeItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortListWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/PortFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/WidgetInlets.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Dataflow/MinMaxFloatPort.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Drop/ProcessDropHandler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Layer/LayerContextMenu.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/OfflineAction/OfflineAction.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Preset.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/MessageNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/State/ProcessStateDataInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Tools/ProcessPanelGraphicsProxy.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionSetup.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/ExecutionAction.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Inspector/ProcessInspectorWidgetDelegate.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Automatable/AutomatableFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/Script/ScriptEditor.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Script/ScriptWidget.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Process/WidgetLayer/WidgetLayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Process/Execution/ProcessComponent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Effect/EffectLayer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Magnetism/MagnetismAdjuster.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Control/DefaultEffectItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Control/Layout.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/LocalTree/LTProcessComponent.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/score_lib_process.cpp"
)

add_library(score_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS})
target_compile_definitions(score_lib_process PUBLIC
    $<$<BOOL:${DEFINE_SCORE_SCENARIO_DEBUG_RECTS}>:SCORE_SCENARIO_DEBUG_RECTS>)

target_include_directories(score_lib_process PUBLIC ${${QT_PREFIX}Widgets_PRIVATE_INCLUDE_DIRS})

target_link_libraries(score_lib_process  PUBLIC
    ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${QT_PREFIX}::WidgetsPrivate score_lib_base score_lib_state
    score_lib_inspector score_lib_device score_lib_localtree)

score_generate_command_list_file(${PROJECT_NAME} "${PROCESS_HDRS}")
setup_score_plugin(score_lib_process)

# setup_score_tests(Tests)
