#pragma once

#include <Scenario/Application/Menus/ScenarioActions.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <QAction>
#include <iscore/actions/Action.hpp>
#include <Process/Layer/LayerContextMenu.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioApplicationPlugin;
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT StateActions : public QObject
{
    public:
        StateActions(ScenarioApplicationPlugin* parent);

        void makeGUIElements(iscore::GUIElements& ref);
        void setupContextMenu(Process::LayerContextMenuManager& ctxm);

    private:
        CommandDispatcher<> dispatcher();

        ScenarioApplicationPlugin* m_parent{};
        QAction* m_updateStates{};
};
}
