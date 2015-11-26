#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>
class RecordManager;
namespace Scenario { class ScenarioModel; }
class IScoreCohesionApplicationPlugin final : public iscore::GUIApplicationContextPlugin
{
    public:
        IScoreCohesionApplicationPlugin(iscore::Application& app);
        void populateMenus(iscore::MenubarManager*) override;
        std::vector<iscore::OrderedToolbar> makeToolbars() override;

        void record(Scenario::ScenarioModel&, Scenario::Point pt);
        void stopRecord();

    private:
        QAction* m_snapshot{};
        QAction* m_curves{};

        QAction* m_stopAction{};

        std::unique_ptr<RecordManager> m_recManager;
};
