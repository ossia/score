cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario)
iscore_common_setup()

option(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS "Enable to have debug rects around elements of a scenario" OFF)
if(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS)
    add_definitions(-DISCORE_SCENARIO_DEBUG_RECTS)
endif()

# Packages
find_package(Qt5 5.5 REQUIRED COMPONENTS Core Widgets Xml Svg Test OpenGL)

### Library ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/source")

file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.hpp")
set(PROCESS_SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/TimeValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/ProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/Focus/FocusDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/Process.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/LayerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/LayerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/LayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ModelMetadataSerialization.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process
    Qt5::Core Qt5::Widgets
    iscore_lib_base iscore_lib_state)

setup_iscore_library(iscore_lib_process)

### Plugin ###
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Control/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/DialogWidget/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Document/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Process/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ViewCommands/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ScenarioPlugin.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/Tools/*.hpp"
)

qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/resources/ScenarioResources.qrc")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/VerticalExtentSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/ConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/LayerModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintDurationsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StateModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioLayerSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseScenarioSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/ScenarioRollbackStrategy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/Slot/CopyLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/Slot/ResizeSlotVertically.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/AddSlotToRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/CopySlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/RemoveSlotFromRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Rack/SwapSlots.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddRackToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddProcessToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddLayerInNewSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/ReplaceConstraintContent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/DuplicateRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/RemoveRackFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/RemoveProcessFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/SetRigidity.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/State/AssignMessagesToState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/State/UnassignMessagesFromState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/AddStateToEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/SetCondition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/SplitEvent.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateEvent_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateTimeNode_Event_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State_Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/RemoveSelection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEventClassicFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEventList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEventMeta.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveNewEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveNewState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/HideRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/ShowRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/SwitchStatePosition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/AddTrigger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/MergeTimeNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/RemoveTrigger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/SplitTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/SetTrigger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/CommandNames.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/ResizeBaseConstraint.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/ViewCommands/PutLayerModelToFront.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/AbstractMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ToolMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ObjectMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/TransportActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/TextDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/Plugin/ScenarioContextMenuPluginList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ScenarioCommonContextMenuFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/ScenarioControl.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/DialogWidget/AddProcessDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/DialogWidget/StateTreeView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseScenarioStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseScenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/DisplayedElements/DisplayedElementsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ProcessFocusManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ZoomPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ScenarioDocument.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/ProgressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SceneGraduations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SizeNotifyingGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotHandle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotOverlay.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/ConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/ConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/ConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/ClickableLabelItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/AddressBarItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/ConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintDurations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/ConditionView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventStatus.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/ModelConsistency.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/Trigger/TriggerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/Trigger/TriggerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/Trigger/TriggerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/AbstractTimeRuler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/AbstractTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/MainTimeRuler/TimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StateView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StateModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelGraphicsProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardCreationPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/GoodOldDisplacementPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardDisplacementPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardRemovalPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/VerticalMovePolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/ScenarioToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioCreation_FromNothing.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/MoveStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ResizeSlotState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/DragSlotState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioCreationState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/ScenarioSelectionState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/CreationToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/MoveSlotToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/SelectionToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/AnythingTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/SlotTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/EventTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/StateTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/NothingTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/ScenarioStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/ScenarioViewInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/AbstractScenarioLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioGlobalCommandManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Tools/dataStructures.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Tools/RandomNameProvider.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/ScenarioPlugin.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/SelectionButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/ConstraintInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Rack/AddSlotWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Rack/Slot/SlotInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Rack/RackInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/LambdaFriendlyQComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/RackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/DurationSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/ConstraintInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/State/StateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/State/StateInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventWidgets/EventShortcut.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/MetadataWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Scenario/ScenarioInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Scenario/ScenarioInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/TimeNode/TimeNodeInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/TimeNode/TimeNodeInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/TimeNode/TriggerInspectorWidget.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()

add_library(iscore_plugin_scenario ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(iscore_plugin_scenario Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Svg Qt5::OpenGL
                     iscore_lib_base
                     iscore_lib_process iscore_plugin_deviceexplorer iscore_lib_state pthread)

if(TARGET iscore_lib_inspector)
    # TODO instead put the definition in the INTERFACE of iscore_lib_inspector
    target_link_libraries(iscore_plugin_scenario iscore_lib_inspector)
endif()

setup_iscore_plugin(iscore_plugin_scenario)
set_property(TARGET iscore_plugin_scenario APPEND
             PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/source")

