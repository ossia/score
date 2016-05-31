#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <memory>
#include <vector>

#include "Record/RecordManager.hpp"
#include "Record/RecordMessagesManager.hpp"

class OSSIAApplicationPlugin;
class QAction;
namespace Scenario {
class ScenarioModel;
struct Point;
}  // namespace Scenario
class IScoreCohesionApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        IScoreCohesionApplicationPlugin(
                const iscore::GUIApplicationContext& app);

        GUIElements makeGUIElements() override;

        void record(const Scenario::ScenarioModel&, Scenario::Point pt);
        void recordMessages(const Scenario::ScenarioModel&, Scenario::Point pt);
        void stopRecord();

    private:
        QAction* m_snapshot{};
        QAction* m_curves{};

        OSSIAApplicationPlugin* m_ossiaplug{};
        QAction* m_stopAction{};

        std::unique_ptr<Recording::RecordManager> m_recManager;
        std::unique_ptr<Recording::RecordMessagesManager> m_recMessagesManager;
};
