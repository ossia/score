#pragma once
#include <iscore/actions/Action.hpp>
#include <Process/Actions/ProcessActions.hpp>

#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Process/Layer/LayerContextMenu.hpp>

namespace Scenario
{

inline const Scenario::ScenarioInterface* focusedScenarioInterface(const iscore::DocumentContext& ctx)
{
    if(auto layer = dynamic_cast<const Process::LayerModel*>(ctx.document.focusManager().get()))
    {
        return dynamic_cast<Scenario::ScenarioInterface*>(&layer->processModel());
    }
    return nullptr;
}
inline const Scenario::ScenarioModel* focusedScenarioModel(const iscore::DocumentContext& ctx)
{
    if(auto layer = dynamic_cast<const Process::LayerModel*>(ctx.document.focusManager().get()))
    {
        return dynamic_cast<Scenario::ScenarioModel*>(&layer->processModel());
    }
    return nullptr;
}

class EnableWhenScenarioModelObject final :
        public iscore::ActionCondition
{
    public:
        EnableWhenScenarioModelObject():
            iscore::ActionCondition{static_key()} { }

        static iscore::ActionConditionKey static_key()
        { return iscore::ActionConditionKey{ "ScenarioModelObject" }; }

    private:
        void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override
        {
            if(!doc)
            {
                setEnabled(mgr, false);
                return;
            }

            auto focus = doc->focus.get();
            if(!focus)
            {
                setEnabled(mgr, false);
                return;
            }

            auto lm = dynamic_cast<const Process::LayerModel*>(focus);
            if(!lm)
            {
                setEnabled(mgr, false);
                return;
            }

            auto proc = dynamic_cast<const Scenario::ScenarioModel*>(&lm->processModel());
            if(!proc)
            {
                setEnabled(mgr, false);
                return;
            }

            const auto& sel = doc->selectionStack.currentSelection();
            auto res = any_of(sel, [] (auto obj) {
                auto ptr = obj.data();
                return bool(dynamic_cast<const Scenario::ConstraintModel*>(ptr))
                    || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
                    || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
            });

            setEnabled(mgr, res);
        }
};


class EnableWhenScenarioInterfaceObject final :
        public iscore::ActionCondition
{
    public:
        EnableWhenScenarioInterfaceObject():
            iscore::ActionCondition{static_key()} { }

        static iscore::ActionConditionKey static_key()
        { return iscore::ActionConditionKey{ "ScenarioInterfaceObject" }; }

    private:
        void action(iscore::ActionManager& mgr, iscore::MaybeDocument doc) override
        {
            if(!doc)
            {
                setEnabled(mgr, false);
                return;
            }

            auto focus = doc->focus.get();
            if(!focus)
            {
                setEnabled(mgr, false);
                return;
            }

            auto lm = dynamic_cast<const Process::LayerModel*>(focus);
            if(!lm)
            {
                setEnabled(mgr, false);
                return;
            }

            auto proc = dynamic_cast<const Scenario::ScenarioInterface*>(&lm->processModel());
            if(!proc)
            {
                setEnabled(mgr, false);
                return;
            }

            const auto& sel = doc->selectionStack.currentSelection();
            auto res = any_of(sel, [] (auto obj) {
                auto ptr = obj.data();
                return bool(dynamic_cast<const Scenario::ConstraintModel*>(ptr))
                    || bool(dynamic_cast<const Scenario::EventModel*>(ptr))
                    || bool(dynamic_cast<const Scenario::StateModel*>(ptr));
            });

            setEnabled(mgr, res);
        }
};
}


/// Conditions relative to Scenario elements
ISCORE_DECLARE_FOCUSED_OBJECT_CONDITION(Scenario::TemporalScenarioLayerModel)
ISCORE_DECLARE_DOCUMENT_CONDITION(Scenario::ScenarioDocumentModel)

ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioModel)
ISCORE_DECLARE_FOCUSED_PROCESS_CONDITION(Scenario::ScenarioInterface)

ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::ConstraintModel)
ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::EventModel)
ISCORE_DECLARE_SELECTED_OBJECT_CONDITION(Scenario::StateModel)


/// Actions
// View
ISCORE_DECLARE_ACTION(SelectAll, "&Select All", Scenario, QKeySequence::SelectAll)
ISCORE_DECLARE_ACTION(DeselectAll, "&Deselect All", Scenario, QKeySequence::Deselect)

// Transport
ISCORE_DECLARE_ACTION(Play, "&Play", Scenario, Qt::Key_Space)
ISCORE_DECLARE_ACTION(Stop, "&Stop", Scenario, Qt::Key_Return)
ISCORE_DECLARE_ACTION(GoToStart, "&Go to Start", Scenario, Qt::Key_Back)
ISCORE_DECLARE_ACTION(GoToEnd, "Go to &End", Scenario, Qt::Key_Forward)
ISCORE_DECLARE_ACTION(Reinitialize, "&Reinitialize", Scenario, Qt::CTRL + Qt::Key_Return)
ISCORE_DECLARE_ACTION(Record, "&Record", Scenario, QKeySequence::UnknownKey)

// Edit
ISCORE_DECLARE_ACTION(SelectTool, "Tool &Select", Scenario, Qt::Key_S)
ISCORE_DECLARE_ACTION_2S(CreateTool, "Tool &Create", Scenario, QKeySequence{QObject::tr("C")}, QKeySequence{QObject::tr("Shift+C")})
ISCORE_DECLARE_ACTION(PlayTool, "Tool &Play",Scenario, Qt::Key_P)
ISCORE_DECLARE_ACTION(SequenceMode, "Se&quence", Scenario, Qt::Key_Shift)

ISCORE_DECLARE_ACTION(Scale, "&Scale mode", Scenario, Qt::ALT + Qt::Key_S)
ISCORE_DECLARE_ACTION(Grow, "&Grow mode", Scenario, Qt::ALT + Qt::Key_D)

// Object
ISCORE_DECLARE_ACTION(RemoveElements, "&Remove elements", Scenario, Qt::Key_Backspace)
ISCORE_DECLARE_ACTION(ClearElements, "C&lear elements", Scenario, Qt::Key_Delete)
ISCORE_DECLARE_ACTION(CopyContent, "C&opy", Scenario, QKeySequence::Copy)
ISCORE_DECLARE_ACTION(CutContent, "C&ut", Scenario, QKeySequence::Cut)
ISCORE_DECLARE_ACTION(PasteContent, "&Paste (content)", Scenario, QKeySequence::Paste)
ISCORE_DECLARE_ACTION(ElementsToJson, "Convert to &JSON", Scenario, QKeySequence::UnknownKey)

// Event
ISCORE_DECLARE_ACTION(AddTrigger, "&Add Trigger", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(RemoveTrigger, "&Remove Trigger", Scenario, QKeySequence::UnknownKey)

// State
ISCORE_DECLARE_ACTION(RefreshStates, "Refresh St&ates", Scenario, Qt::CTRL + Qt::Key_U)

// Constraint
ISCORE_DECLARE_ACTION(AddProcess, "&Add a process", Scenario, QKeySequence::UnknownKey)
ISCORE_DECLARE_ACTION(InterpolateStates, "&Interpolate states", Scenario, Qt::CTRL + Qt::Key_K)
ISCORE_DECLARE_ACTION(MergeTimeNodes, "&Merge TimeNodes", Scenario, Qt::SHIFT + Qt::Key_M)

/// Context menus
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(ISCORE_PLUGIN_SCENARIO_EXPORT, ScenarioObjectContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(ISCORE_PLUGIN_SCENARIO_EXPORT, ScenarioModelContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(ISCORE_PLUGIN_SCENARIO_EXPORT, ConstraintContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(ISCORE_PLUGIN_SCENARIO_EXPORT, EventContextMenu)
ISCORE_PROCESS_DECLARE_CONTEXT_MENU(ISCORE_PLUGIN_SCENARIO_EXPORT, StateContextMenu)
