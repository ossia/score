cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface" PARENT_SCOPE)
set(SCENARIO_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source" PARENT_SCOPE)

option(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS "Enable to have debug rects around elements of a scenario" OFF)
if(DEFINE_ISCORE_SCENARIO_DEBUG_RECTS)
    add_definitions(-DISCORE_SCENARIO_DEBUG_RECTS)
endif()

# Packages
find_package(Qt5 5.4 REQUIRED COMPONENTS Core Widgets Xml Svg Test)

### Library ###
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/source")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/")
include_directories("${DEVICEEXPLORER_INTERFACE_FOLDER}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../") # API
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../../API/Headers/")

file(GLOB_RECURSE PROCESS_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/*.hpp")
set(PROCESS_SRCS

"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/TimeValueSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/ProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/Focus/FocusDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/Layer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/LayerModel.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process Qt5::Core Qt5::Widgets iscore_lib_base)
install(TARGETS iscore_lib_process
    LIBRARY DESTINATION ./
    ARCHIVE DESTINATION static_lib/)

if(ISCORE_COTIRE)
    cotire(iscore_lib_process)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(iscore_lib_process PROPERTIES COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
endif()

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
)
file(GLOB_RECURSE QRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.qrc")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/VerticalExtentSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/LayerModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/ModelMetadataSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/DisplayedStateModelSerialization.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/CopyConstraintContent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/DuplicateRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/RemoveRackFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/RemoveProcessFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/SetMaxDuration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/SetMinDuration.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/SetRigidity.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/State/AssignMessagesToState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/State/UnassignMessagesFromState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/AddStateToEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/RemoveStateFromEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/SetCondition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Event/SetTrigger.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateEvent_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State_Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/RemoveSelection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveNewEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveNewState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/HideRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/ResizeConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/ShowRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/SwitchStatePosition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/MergeTimeNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/SplitTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/CommandNames.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/ResizeBaseConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/ViewCommands/PutLayerModelToFront.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/AbstractMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ToolMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ObjectMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/Plugin/ScenarioContextMenuPluginList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ScenarioCommonContextMenuFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/OldFormatConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/ScenarioControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/DialogWidget/AddProcessDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseElementStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseScenarioModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseScenario/BaseScenarioPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/DisplayedElementsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ProcessFocusManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ScenarioDocument.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/StateMachine/BaseMoveSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/AddressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/ClickableLabel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/ProgressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SceneGraduations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SizeNotifyingGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotHandle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/Slot/SlotOverlay.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Rack/RackPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/AbstractConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/AbstractConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/AbstractConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/ConditionView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/TriggerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/ModelConsistency.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/ModelMetadata.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/AbstractTimeRuler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/AbstractTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeRuler/MainTimeRuler/TimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StateView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/StatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/State/DisplayedStateModel.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelGraphicsProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardCreationPolicy.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/SlotStates.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioLayer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/ScenarioViewInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/AbstractScenarioViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioGlobalCommandManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/ScenarioPlugin.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
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
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()

add_library(iscore_plugin_scenario ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(iscore_plugin_scenario Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Svg
                     iscore_lib_base
                     iscore_lib_process iscore_plugin_deviceexplorer iscore_lib_state pthread)

if(TARGET iscore_lib_inspector)
    # TODO instead put the definition in the INTERFACE of iscore_lib_inspector
    target_link_libraries(iscore_plugin_scenario iscore_lib_inspector)
endif()


if(ISCORE_COTIRE)
    cotire(iscore_plugin_scenario)
    if(ISCORE_COTIRE_ALL_HEADERS)
        set_target_properties(iscore_plugin_scenario PROPERTIES
                              COTIRE_PREFIX_HEADER_IGNORE_PATH "")
    endif()
        set_target_properties(iscore_plugin_scenario PROPERTIES
                              COTIRE_PREFIX_HEADER_IGNORE_PATH "${COTIRE_PREFIX_HEADER_IGNORE_PATH};/usr/include/boost/preprocessor/")
endif()

install(TARGETS iscore_plugin_scenario
    LIBRARY DESTINATION plugins/
    ARCHIVE DESTINATION static_plugins/)

#add_subdirectory(tests)
