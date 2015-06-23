cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(SCENARIO_PROCESS_INTERFACE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterface" PARENT_SCOPE)

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
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/Focus/FocusDispatcher.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/ProcessInterface/ProcessViewModel.cpp"
)

add_library(iscore_lib_process ${PROCESS_HDRS} ${PROCESS_SRCS} )
target_link_libraries(iscore_lib_process Qt5::Core Qt5::Widgets iscore_lib_base)
install(TARGETS iscore_lib_process
    LIBRARY DESTINATION ./
    ARCHIVE DESTINATION static_lib/)

cotire(iscore_lib_process)
set_target_properties(iscore_lib_process
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")

### Plugin ###
file(GLOB_RECURSE HDRS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.hpp")
file(GLOB_RECURSE QRCS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.qrc")

set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/TimeValueSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/AbstractConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/ProcessModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/source/ProcessInterfaceSerialization/ProcessViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/ModelMetadataSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/BoxModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/ConstraintModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Event/EventModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/TimeNode/TimeNodeModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/Deck/AddProcessViewModelToDeck.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/Deck/CopyProcessViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/Deck/RemoveProcessViewModelFromDeck.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/Deck/ResizeDeckVertically.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/AddDeckToBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/CopyDeck.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/RemoveDeckFromBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/Box/SwapDecks.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddBoxToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddProcessToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/AddProcessViewInNewDeck.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/CopyConstraintContent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/DuplicateBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Constraint/RemoveBoxFromConstraint.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateEventAfterEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Creations/CreateEventOnTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/ClearEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Deletions/RemoveSelection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveEventAndConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/Displacement/MoveTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/HideBoxInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/ResizeConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/Scenario/ShowBoxInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/SwitchStatePosition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/MergeTimeNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/TimeNode/SplitTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/CommandNames.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Commands/ResizeBaseConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/AbstractMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ToolMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/Menus/ObjectMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/OldFormatConversion.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Control/ScenarioControl.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/DialogWidget/AddProcessDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ProcessFocusManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/ScenarioDocument.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/StateMachine/BaseMoveDeck.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/AddressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/ClickableLabel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/DoubleSlider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/ProgressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SceneGraduations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/Widgets/SizeNotifyingGraphicsView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/BaseElement/BaseElementPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/BoxView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckHandle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/Deck/DeckOverlay.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/BoxModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Document/Constraint/Box/BoxPresenter.cpp"
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
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/ConstraintInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Box/AddDeckWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Box/Deck/AddProcessViewModelWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Box/Deck/DeckInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/Box/BoxInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/LambdaFriendlyQComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/BoxWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/Widgets/DurationSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Constraint/ConstraintInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventWidgets/EventShortcut.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Event/EventInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/MetadataWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Scenario/ScenarioInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/Scenario/ScenarioInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/TimeNode/TimeNodeInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Inspector/TimeNode/TimeNodeInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelGraphicsProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Panel/ProcessPanelView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardCreationPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardDisplacementPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Algorithms/StandardRemovalPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/MoveToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/ScenarioToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/CreateEventState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/MoveStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/States/DeckStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/CreationToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/MoveDeckToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Tools/SelectionToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/AnythingTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/ConstraintTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/DeckTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/EventTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/NothingTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/Transitions/TimeNodeTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/StateMachines/ScenarioStateMachine.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/ScenarioViewInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/Temporal/TemporalScenarioPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/AbstractScenarioViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioGlobalCommandManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/Process/ScenarioModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/source/ScenarioPlugin.cpp"
)

add_library(iscore_plugin_scenario ${SRCS} ${HDRS} ${QRCS})
target_link_libraries(iscore_plugin_scenario Qt5::Core Qt5::Widgets Qt5::Xml Qt5::Svg
                     iscore_lib_base
                     iscore_lib_inspector iscore_lib_process iscore_plugin_deviceexplorer iscore_lib_state pthread)

cotire(iscore_plugin_scenario)
set_target_properties(iscore_plugin_scenario
                      PROPERTIES
                        COTIRE_PREFIX_HEADER_IGNORE_PATH "")

install(TARGETS iscore_plugin_scenario
    LIBRARY DESTINATION plugins/
    ARCHIVE DESTINATION static_plugins/)

#add_subdirectory(tests)
