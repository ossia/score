#pragma once
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <memory>
#include <vector>

#include "Record/RecordManager.hpp"
#include "Record/RecordMessagesManager.hpp"
namespace Engine
{
class ApplicationPlugin;
}
class QAction;
namespace Scenario {
class ProcessModel;
struct Point;
}  // namespace Scenario
namespace Recording
{
class ApplicationPlugin final :
        public QObject,
        public iscore::GUIApplicationContextPlugin
{
    public:
        ApplicationPlugin(
                const iscore::GUIApplicationContext& app);

        void record(const Scenario::ProcessModel&, Scenario::Point pt);
        void recordMessages(const Scenario::ProcessModel&, Scenario::Point pt);
        void stopRecord();

    private:
        Engine::ApplicationPlugin* m_ossiaplug{};
        QAction* m_stopAction{};

        std::unique_ptr<Recording::RecordManager> m_recManager;
        std::unique_ptr<Recording::RecordMessagesManager> m_recMessagesManager;
};
}
