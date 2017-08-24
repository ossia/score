#pragma once
#include <Process/Actions/ProcessActions.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/actions/Action.hpp>

#include <Process/Layer/LayerContextMenu.hpp>

namespace Scenario
{
class ScenarioInterface;
class ScenarioDocumentModel;
class ProcessModel;
ISCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ScenarioInterface*
focusedScenarioInterface(const iscore::DocumentContext& ctx);
ISCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ProcessModel*
focusedScenarioModel(const iscore::DocumentContext& ctx);

//! Anything in a scenario model
class EnableWhenScenarioModelObject final : public iscore::ActionCondition
{
public:
  EnableWhenScenarioModelObject();

  static iscore::ActionConditionKey static_key();

private:
  void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override;
};

//! Only events, nodes, states
class EnableWhenScenarioInterfaceInstantObject final : public iscore::ActionCondition
{
public:
  EnableWhenScenarioInterfaceInstantObject();

  static iscore::ActionConditionKey static_key();

private:
  void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override;
};

//! Anything in a scenario interface
class EnableWhenScenarioInterfaceObject final : public iscore::ActionCondition
{
public:
  EnableWhenScenarioInterfaceObject();

  static iscore::ActionConditionKey static_key();

private:
  void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override;
};
}

/// Conditions relative to Scenario elements
ISCORE_DECLARE_DOCUMENT_CONDITION(Scenario::ScenarioDocumentModel)

ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ProcessModel)
ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioInterface)

ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::ConstraintModel)
ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::EventModel)
ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::StateModel)
ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::TimeSyncModel)

/// Actions
// View
ISCORE_DECLARE_ACTION(
    SelectAll, "&Select All", Scenario, QKeySequence::SelectAll)
ISCORE_DECLARE_ACTION(
    DeselectAll, "&Deselect All", Scenario, QKeySequence::Deselect)
ISCORE_DECLARE_ACTION(
    SelectTop, "Select &Top", Scenario, QKeySequence{QObject::tr("Ctrl+Shift+T")})

// Transport
ISCORE_DECLARE_ACTION(Play, "&Play", Scenario, Qt::Key_Space)
ISCORE_DECLARE_ACTION(Stop, "&Stop", Scenario, Qt::Key_Return)
ISCORE_DECLARE_ACTION(GoToStart, "&Go to Start", Scenario, Qt::Key_Back)
ISCORE_DECLARE_ACTION(GoToEnd, "Go to &End", Scenario, Qt::Key_Forward)
ISCORE_DECLARE_ACTION(
    Reinitialize, "&Reinitialize", Scenario, Qt::CTRL + Qt::Key_Return)
ISCORE_DECLARE_ACTION(Record, "&Record", Scenario, QKeySequence::UnknownKey)

// Edit
ISCORE_DECLARE_ACTION(SelectTool, "Tool &Select", Scenario, Qt::Key_S)
ISCORE_DECLARE_ACTION_2S(
    CreateTool, "Tool &Create", Scenario, QKeySequence{QObject::tr("C")},
    QKeySequence{QObject::tr("Shift+C")})
ISCORE_DECLARE_ACTION(PlayTool, "Tool &Play", Scenario, Qt::Key_P)
ISCORE_DECLARE_ACTION(SequenceMode, "Se&quence", Scenario, Qt::Key_Shift)

ISCORE_DECLARE_ACTION(Scale, "&Scale mode", Scenario, Qt::ALT + Qt::Key_S)
ISCORE_DECLARE_ACTION(Grow, "&Grow mode", Scenario, Qt::ALT + Qt::Key_D)

// Object
ISCORE_DECLARE_ACTION(
    RemoveElements, "&Remove elements", Scenario, Qt::Key_Backspace)
ISCORE_DECLARE_ACTION(
    ClearElements, "C&lear elements", Scenario, Qt::Key_Delete)
ISCORE_DECLARE_ACTION(CopyContent, "C&opy", Scenario, QKeySequence::Copy)
ISCORE_DECLARE_ACTION(CutContent, "C&ut", Scenario, QKeySequence::Cut)
ISCORE_DECLARE_ACTION(
    PasteContent, "&Paste (content)", Scenario, QKeySequence::Print)
ISCORE_DECLARE_ACTION(
    PasteElements, "&Paste (elements)", Scenario, QKeySequence::Paste)
ISCORE_DECLARE_ACTION(
    ElementsToJson, "Convert to &JSON", Scenario, QKeySequence::UnknownKey)

// Event
ISCORE_DECLARE_ACTION(
    AddTrigger, "&Enable Trigger", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(
    RemoveTrigger, "&Disable Trigger", Scenario, QKeySequence::UnknownKey)

ISCORE_DECLARE_ACTION(
    AddCondition, "&Add Condition", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(
    RemoveCondition, "&Remove Condition", Scenario, QKeySequence::UnknownKey)

// State
ISCORE_DECLARE_ACTION(
    RefreshStates, "Refresh St&ates", Scenario, Qt::CTRL + Qt::Key_U)
ISCORE_DECLARE_ACTION(
    Snapshot, "Snapshot in Event", Scenario,
    QKeySequence(QObject::tr("Ctrl+L")))

// Constraint
ISCORE_DECLARE_ACTION(
    AddProcess, "&Add a process", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(
    InterpolateStates, "&Interpolate states", Scenario, Qt::CTRL + Qt::Key_K)
ISCORE_DECLARE_ACTION(
    CreateCurves, "Create Curves", Scenario,
    QKeySequence(QObject::tr("Ctrl+J")))
ISCORE_DECLARE_ACTION(
    MergeTimeSyncs, "&Merge TimeSyncs", Scenario, Qt::SHIFT + Qt::Key_M)
ISCORE_DECLARE_ACTION(
    ShowRacks, "&Show racks", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(
    HideRacks, "&Hide racks", Scenario, QKeySequence::UnknownKey)

// Navigation
ISCORE_DECLARE_ACTION(MoveUp, "&Move up", Scenario, Qt::UpArrow)
ISCORE_DECLARE_ACTION(MoveDown, "&Move down", Scenario, Qt::DownArrow)
ISCORE_DECLARE_ACTION(MoveLeft, "&Move left", Scenario, Qt::LeftArrow)
ISCORE_DECLARE_ACTION(MoveRight, "&Move right", Scenario, Qt::RightArrow)

/// Context menus
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(
    ISCORE_PLUGIN_SCENARIO_EXPORT, ScenarioObjectContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(
    ISCORE_PLUGIN_SCENARIO_EXPORT, ScenarioModelContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(
    ISCORE_PLUGIN_SCENARIO_EXPORT, ConstraintContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(
    ISCORE_PLUGIN_SCENARIO_EXPORT, EventContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(
    ISCORE_PLUGIN_SCENARIO_EXPORT, StateContextMenu)
