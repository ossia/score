#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <memory>
#include <vector>

#include "Record/RecordManager.hpp"

class QAction;
namespace Scenario {
class ScenarioModel;
struct Point;
}  // namespace Scenario
namespace iscore {
class Application;
class MenubarManager;
struct OrderedToolbar;
}  // namespace iscore

class IScoreCohesionApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
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
