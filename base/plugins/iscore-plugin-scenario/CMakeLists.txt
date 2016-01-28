cmake_minimum_required(VERSION 3.0)
project(iscore_plugin_scenario LANGUAGES CXX)
iscore_common_setup()

# Packages
# TODO make a check to see what libs aren't necessary
find_package(Qt5 5.3 REQUIRED COMPONENTS Core Widgets Xml Svg Test)
if(ISCORE_OPENGL)
find_package(Qt5 5.3 REQUIRED COMPONENTS OpenGL)
endif()

if(ISCORE_WEBSOCKETS)
find_package(Qt5 5.3 REQUIRED COMPONENTS WebSockets)
endif()

### Dummy process ###
add_subdirectory(DummyProcess)

### Scenario plugin ###
file(GLOB_RECURSE HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/DialogWidget/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Palette/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Panel/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/ViewCommands/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/iscore_plugin_scenario.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Tools/*.hpp"
)

list(REMOVE_ITEM HDRS "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/WebSocketView.hpp")
qt5_add_resources(QRCS "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/resources/ScenarioResources.qrc")

if(ISCORE_WEBSOCKETS)
set(SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/WebSocketView.cpp"
)
set(HDRS ${HDRS}
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/WebSocketView.hpp"
)
endif()
set(SRCS ${SRCS}
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/VerticalExtentSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/LayerModelLoader.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModelSerialization.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/RackModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ConstraintDurationsSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ConstraintModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/StateModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/EventModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/TimeNodeModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/CommentBlock/CommentBlockModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/TemporalScenarioLayerSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/ScenarioModelSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/BaseScenario/BaseScenarioContainerSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/BaseScenario/BaseScenarioSerialization.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ScenarioDocumentModelSerialization.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Palette/Tools/ScenarioRollbackStrategy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/SetLooping.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/Slot/RemoveLayerModelFromSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/Slot/ResizeSlotVertically.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/AddSlotToRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/CopySlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/RemoveSlotFromRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/Rack/SwapSlots.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/AddRackToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/AddOnlyProcessToConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/AddLayerInNewSlot.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/InsertContentInConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/DuplicateRack.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/RemoveRackFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/RemoveProcessFromConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Constraint/SetRigidity.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Event/SetCondition.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Event/SplitEvent.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Comment/SetCommentText.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateEvent_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateConstraint_State.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateSequence.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Creations/CreateCommentBlock.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Deletions/ClearConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Deletions/ClearState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Deletions/RemoveSelection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveConstraint.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveEventClassicFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveEventFactoryInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveEventList.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveEventMeta.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveEventOnCreationMeta.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveNewEvent.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveNewState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/Displacement/MoveCommentBlock.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/HideRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/ShowRackInViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/ShowRackInAllViewModels.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Scenario/ScenarioPasteElements.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Cohesion/CreateCurveFromStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Cohesion/InterpolateStates.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/Cohesion/RefreshStates.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/State/RemoveMessageNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/State/AddMessagesToState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/State/InsertContentInState.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/TriggerCommandFactory/TriggerCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/TriggerCommandFactory/ScenarioTriggerCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/TriggerCommandFactory/BaseScenarioTriggerCommandFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/MergeTimeNodes.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/SplitTimeNode.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/TimeNode/SetTrigger.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/CommandNames.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/SetProcessDuration.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Commands/ScenarioCommandFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/ViewCommands/PutLayerModelToFront.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/Plugin/ScenarioActionsFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ScenarioActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ToolMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ScenarioCopy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ObjectMenuActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/TransportActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/TextDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ScenarioCommonContextMenuFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ScenarioContextMenuManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ObjectsActions/EventActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ObjectsActions/ConstraintActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/Menus/ObjectsActions/StateActions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/ScenarioApplicationPlugin.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Application/ScenarioEditionSettings.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/DialogWidget/AddProcessDialog.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/DialogWidget/MessageTreeView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/BaseScenario/BaseScenario.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/BaseScenario/BaseElementContext.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ProcessFocusManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ZoomPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/Widgets/ProgressBar.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/Widgets/SceneGraduations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ScenarioDocumentModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ScenarioDocument/ScenarioDocumentView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/ScenarioDisplayedElementsProvider.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/BaseScenarioDisplayedElementsProvider.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/DisplayedElementsToolPaletteFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElementsToolPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElementsToolPaletteFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/ScenarioDisplayedElementsToolPaletteFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/RackView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/BaseScenario/BaseScenarioContainer.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotHandle.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/Slot/SlotOverlay.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/RackModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/Rack/RackPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/ConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/ConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/ConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/ClickableLabelItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/FullView/AddressBarItem.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/Braces/ConstraintBrace.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/Braces/LeftBrace.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/Braces/RightBrace.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintHeader.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ViewModels/ConstraintView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ConstraintDurations.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Constraint/ConstraintModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/ConditionView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/EventModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/EventPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/EventView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/Event/ExecutionStatus.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/ModelConsistency.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/TimeNodeModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/TimeNodePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/TimeNodeView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/Trigger/TriggerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/Trigger/TriggerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeNode/Trigger/TriggerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/AbstractTimeRuler.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/AbstractTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/LocalTimeRuler/LocalTimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/TimeRuler/MainTimeRuler/TimeRulerView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/StateView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/StatePresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/StateModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/ItemModel/MessageItemModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/CommentBlock/CommentBlockModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/CommentBlock/CommentBlockPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Document/CommentBlock/CommentBlockView.cpp"


"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Panel/ProcessPanelFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Panel/ProcessPanelModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Panel/ProcessPanelPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Panel/ProcessPanelView.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Algorithms/StandardCreationPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Algorithms/GoodOldDisplacementPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Algorithms/StandardRemovalPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Algorithms/VerticalMovePolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Algorithms/ProcessPolicy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/ScenarioFactory.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Palette/Tools/MoveSlotToolState.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Palette/Transitions/SlotTransitions.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Palette/ScenarioPalette.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/TemporalScenarioView.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/TemporalScenarioLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/ScenarioViewInterface.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/TemporalScenarioPresenter.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/Temporal/TemporalScenarioPanelProxy.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/AbstractScenarioLayerModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/ScenarioGlobalCommandManager.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/ScenarioModel.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Process/ScenarioInterface.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Tools/dataStructures.cpp"

"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/iscore_plugin_scenario.cpp"
)

if(TARGET iscore_lib_inspector)
    set(INSPECTOR_SRCS
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/SelectionButton.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ConstraintInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/BaseConstraintInspectorDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ScenarioConstraintInspectorDelegateFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/Rack/AddSlotWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/Rack/Slot/AddLayerModelWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/Rack/Slot/SlotInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/Rack/RackInspectorSection.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/LambdaFriendlyQComboBox.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/RackWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/DurationSectionWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/Widgets/ProcessTabWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ConstraintInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ConstraintInspectorDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/BaseConstraintInspectorDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Constraint/ScenarioConstraintInspectorDelegate.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/State/StateInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/State/StateInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Event/EventInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Event/EventWidgets/EventShortcut.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Event/EventInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/MetadataWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Scenario/ScenarioInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Scenario/ScenarioInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/TimeNode/TimeNodeInspectorFactory.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/TimeNode/TimeNodeInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/TimeNode/TriggerInspectorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Expression/ExpressionEditorWidget.cpp"
"${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/Expression/SimpleExpressionEditorWidget.cpp"
    )

    set(SRCS ${SRCS} ${INSPECTOR_SRCS})

    file(GLOB_RECURSE INSPECTOR_HDRS
        "${CMAKE_CURRENT_SOURCE_DIR}/Scenario/Inspector/*.hpp")

    set(HDRS ${HDRS} ${INSPECTOR_HDRS})
endif()

add_library(iscore_plugin_scenario ${SRCS} ${HDRS} ${QRCS})
iscore_generate_command_list_file(iscore_plugin_scenario "${HDRS}")

target_link_libraries(iscore_plugin_scenario
	PUBLIC
		Qt5::Core Qt5::Widgets Qt5::Svg
		iscore_lib_base
		iscore_lib_process iscore_lib_device iscore_plugin_deviceexplorer iscore_lib_state iscore_plugin_curve iscore_plugin_automation pthread)

if(ISCORE_OPENGL)
target_link_libraries(iscore_plugin_scenario PRIVATE Qt5::OpenGL)
endif()

if(ISCORE_WEBSOCKETS)
target_link_libraries(iscore_plugin_scenario PRIVATE Qt5::WebSockets)
endif()

if(TARGET iscore_lib_inspector)
	target_link_libraries(iscore_plugin_scenario PUBLIC iscore_lib_inspector)
endif()

setup_iscore_plugin(iscore_plugin_scenario)

