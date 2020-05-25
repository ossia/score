#pragma once
#include <Process/Actions/ProcessActions.hpp>
#include <Process/Layer/LayerContextMenu.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/actions/Action.hpp>

namespace Scenario
{
class ScenarioInterface;
class ScenarioDocumentModel;
class ProcessModel;
SCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ScenarioInterface*
focusedScenarioInterface(const score::DocumentContext& ctx);
SCORE_PLUGIN_SCENARIO_EXPORT const Scenario::ProcessModel*
focusedScenarioModel(const score::DocumentContext& ctx);

//! Anything in a scenario model
class SCORE_PLUGIN_SCENARIO_EXPORT EnableWhenScenarioModelObject final
    : public score::ActionCondition
{
public:
  EnableWhenScenarioModelObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};

//! Only events, nodes, states
class EnableWhenScenarioInterfaceInstantObject final : public score::ActionCondition
{
public:
  EnableWhenScenarioInterfaceInstantObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};

//! Anything in a scenario interface
class EnableWhenScenarioInterfaceObject final : public score::ActionCondition
{
public:
  EnableWhenScenarioInterfaceObject();

  static score::ActionConditionKey static_key();

private:
  void action(score::ActionManager& mgr, score::MaybeDocument doc) override;
};
}

/// Conditions relative to Scenario elements
SCORE_DECLARE_DOCUMENT_CONDITION(Scenario::ScenarioDocumentModel)

SCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ProcessModel)
SCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioInterface)

SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::IntervalModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::EventModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::StateModel)
SCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::TimeSyncModel)

/// Actions
// View
SCORE_DECLARE_ACTION(SelectAll, "&Select All", Scenario, QKeySequence::SelectAll)
SCORE_DECLARE_ACTION(DeselectAll, "&Deselect All", Scenario, QKeySequence::Deselect)
SCORE_DECLARE_ACTION(SelectTop, "Select &Top", Scenario, QKeySequence{QObject::tr("Ctrl+Shift+T")})

// Transport
SCORE_DECLARE_ACTION(Play, "&Play", Scenario, Qt::Key_Space)
SCORE_DECLARE_ACTION(PlayGlobal, "&Play Root", Scenario, Qt::Key_Shift + Qt::Key_Space)
SCORE_DECLARE_ACTION(Stop, "&Stop", Scenario, Qt::Key_Return)
SCORE_DECLARE_ACTION(GoToStart, "&Go to Start", Scenario, Qt::Key_Back)
SCORE_DECLARE_ACTION(GoToEnd, "Go to &End", Scenario, Qt::Key_Forward)
SCORE_DECLARE_ACTION(Reinitialize, "&Reinitialize", Scenario, Qt::CTRL + Qt::Key_Return)
SCORE_DECLARE_ACTION(Record, "&Record", Scenario, QKeySequence::UnknownKey)

// Edit
SCORE_DECLARE_ACTION(SelectTool, "Tool &Select", Scenario, Qt::Key_S)
SCORE_DECLARE_ACTION_2S(
    CreateTool,
    "Tool &Create",
    Scenario,
    QKeySequence{QObject::tr("C")},
    QKeySequence{QObject::tr("Shift+C")})
SCORE_DECLARE_ACTION(PlayTool, "Tool &Play", Scenario, Qt::Key_P)
SCORE_DECLARE_ACTION(SequenceMode, "Se&quence", Scenario, Qt::Key_Shift)
SCORE_DECLARE_ACTION(LockMode, "&Lock", Scenario, QKeySequence{QObject::tr("Alt")})

SCORE_DECLARE_ACTION(Scale, "&Scale mode", Scenario, Qt::ALT + Qt::Key_S)
SCORE_DECLARE_ACTION(Grow, "&Grow mode", Scenario, Qt::ALT + Qt::Key_D)

// Object
#if defined(__APPLE__)
SCORE_DECLARE_ACTION(RemoveElements, "&Remove elements", Scenario, Qt::Key_Backspace)
#else
SCORE_DECLARE_ACTION(RemoveElements, "&Remove elements", Scenario, Qt::Key_Delete)
#endif
SCORE_DECLARE_ACTION(CopyContent, "C&opy", Scenario, QKeySequence::Copy)
SCORE_DECLARE_ACTION(CutContent, "C&ut", Scenario, QKeySequence::Cut)
SCORE_DECLARE_ACTION(PasteElements, "&Paste (elements)", Scenario, QKeySequence::Paste)
SCORE_DECLARE_ACTION(PasteElementsAfter, "&Paste (after)", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(ElementsToJson, "Convert to &JSON", Scenario, QKeySequence::UnknownKey)

// Event
SCORE_DECLARE_ACTION(MergeEvents, "Merge events", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(AddTrigger, "&Enable Trigger", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(RemoveTrigger, "&Disable Trigger", Scenario, QKeySequence::UnknownKey)

SCORE_DECLARE_ACTION(AddCondition, "&Add Condition", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(RemoveCondition, "&Remove Condition", Scenario, QKeySequence::UnknownKey)

// Interval
SCORE_DECLARE_ACTION(AddProcess, "&Add a process", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(MergeTimeSyncs, "&Synchronize", Scenario, Qt::SHIFT + Qt::Key_M)
SCORE_DECLARE_ACTION(ShowRacks, "&Show processes", Scenario, QKeySequence::UnknownKey)
SCORE_DECLARE_ACTION(HideRacks, "&Hide processes", Scenario, QKeySequence::UnknownKey)

SCORE_DECLARE_ACTION(Encapsulate, "&Encapsulate", Scenario, Qt::SHIFT + Qt::CTRL + Qt::Key_E)
SCORE_DECLARE_ACTION(Decapsulate, "&Decapsulate", Scenario, Qt::SHIFT + Qt::CTRL + Qt::Key_D)
SCORE_DECLARE_ACTION(FoldIntervals, "Fold intervals", Scenario, Qt::CTRL + Qt::ALT + Qt::Key_F)
SCORE_DECLARE_ACTION(UnfoldIntervals, "Unfold intervals", Scenario, Qt::CTRL + Qt::ALT + Qt::Key_U)
SCORE_DECLARE_ACTION(LevelUp, "Go to parent interval", Scenario, Qt::CTRL + Qt::ALT + Qt::Key_Up)

SCORE_DECLARE_ACTION(Duplicate, "&Duplicate", Scenario, Qt::ALT + Qt::Key_D)

SCORE_DECLARE_ACTION(ShowCables, "&Show cables", Dataflow, Qt::ALT + Qt::SHIFT + Qt::Key_G)

// Navigation
SCORE_DECLARE_ACTION(MoveUp, "&Move up", Scenario, Qt::UpArrow)
SCORE_DECLARE_ACTION(MoveDown, "&Move down", Scenario, Qt::DownArrow)
SCORE_DECLARE_ACTION(MoveLeft, "&Move left", Scenario, Qt::LeftArrow)
SCORE_DECLARE_ACTION(MoveRight, "&Move right", Scenario, Qt::RightArrow)
SCORE_DECLARE_ACTION(GoToParent, "&Go to parent", Scenario, Qt::CTRL + Qt::Key_Up)

/// Context menus
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, ScenarioObjectContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, ScenarioModelContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, IntervalContextMenu)
SCORE_PROCESS_DECLARE_CONTEXT_MENU(SCORE_PLUGIN_SCENARIO_EXPORT, EventContextMenu)
