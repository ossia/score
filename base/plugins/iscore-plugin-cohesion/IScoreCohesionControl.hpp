#pragma once
#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
class RecordManager;
class ScenarioModel;
class IScoreCohesionControl final : public iscore::PluginControlInterface
{
    public:
        IScoreCohesionControl(iscore::Presenter* pres);
        void populateMenus(iscore::MenubarManager*) override;
        QList<iscore::OrderedToolbar> makeToolbars() override;

        void record(ScenarioModel&, Scenario::Point pt);
        void stopRecord();

    private:
        QAction* m_snapshot{};
        QAction* m_curves{};

        QAction* m_stopAction{};

        std::unique_ptr<RecordManager> m_recManager;
};
