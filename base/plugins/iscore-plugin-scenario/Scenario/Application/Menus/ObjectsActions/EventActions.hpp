#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore_plugin_scenario_export.h>
#include <iscore/actions/Action.hpp>

namespace Process
{
class LayerContextMenuManager;
}

namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
namespace Command
{
class TriggerCommandFactoryList;
}
class ISCORE_PLUGIN_SCENARIO_EXPORT EventActions : public QObject
{
    public:
        EventActions(ScenarioApplicationPlugin* parent);

        void makeGUIElements(iscore::GUIElements& ref);
        void setupContextMenu(Process::LayerContextMenuManager& ctxm);

    private:
        void addTriggerToTimeNode();
        void removeTriggerFromTimeNode();

        CommandDispatcher<> dispatcher();

        ScenarioApplicationPlugin* m_parent{};
        QAction *m_addTrigger{};
        QAction *m_removeTrigger{};

        const Command::TriggerCommandFactoryList& m_triggerCommandFactory;
};
}
