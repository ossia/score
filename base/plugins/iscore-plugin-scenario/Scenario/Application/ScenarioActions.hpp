#pragma once
#include <iscore/actions/Action.hpp>
#include <Process/Actions/ProcessActions.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

ISCORE_DECLARE_ACTION(SelectAll, Scenario, QKeySequence::SelectAll)
ISCORE_DECLARE_ACTION(DeselectAll, Scenario, QKeySequence::Deselect)

ISCORE_DECLARE_FOCUSED_OBJECT_CONDITION(Scenario::TemporalScenarioLayerModel)
ISCORE_DECLARE_DOCUMENT_CONDITION(Scenario::ScenarioDocumentModel)

ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioModel)
ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioInterface)

// Transport
ISCORE_DECLARE_ACTION(Play, Scenario, Qt::Key_Space)
ISCORE_DECLARE_ACTION(Stop, Scenario, Qt::Key_Return)
ISCORE_DECLARE_ACTION(GoToStart, Scenario, Qt::Key_Back)
ISCORE_DECLARE_ACTION(GoToEnd, Scenario, Qt::Key_Forward)
ISCORE_DECLARE_ACTION(Reinitialize, Scenario, Qt::CTRL + Qt::Key_Return)
ISCORE_DECLARE_ACTION(Record, Scenario, QKeySequence::UnknownKey)

// Edit
ISCORE_DECLARE_ACTION(SelectTool, Scenario, Qt::Key_S)
ISCORE_DECLARE_ACTION(CreateTool, Scenario, Qt::Key_C)
ISCORE_DECLARE_ACTION(PlayTool, Scenario, Qt::Key_P)
ISCORE_DECLARE_ACTION(SequenceMode, Scenario, Qt::Key_Shift)

ISCORE_DECLARE_ACTION(Scale, Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(Grow, Scenario, QKeySequence::UnknownKey)

// Object
ISCORE_DECLARE_ACTION(RemoveElements, Scenario, Qt::Key_Backspace)
ISCORE_DECLARE_ACTION(ClearElements, Scenario, Qt::Key_Delete)
ISCORE_DECLARE_ACTION(CopyContent, Scenario, QKeySequence::Copy)
ISCORE_DECLARE_ACTION(CutContent, Scenario, QKeySequence::Cut)
ISCORE_DECLARE_ACTION(PasteContent, Scenario, QKeySequence::Paste)
ISCORE_DECLARE_ACTION(ElementsToJson, Scenario, QKeySequence::UnknownKey)

// Event
ISCORE_DECLARE_ACTION(AddTrigger, Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(RemoveTrigger, Scenario, QKeySequence::UnknownKey)

// State
ISCORE_DECLARE_ACTION(RefreshStates, Scenario, QKeySequence::UnknownKey)

// Constraint
ISCORE_DECLARE_ACTION(AddProcess, Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(InterpolateStates, Scenario, QKeySequence::UnknownKey)
