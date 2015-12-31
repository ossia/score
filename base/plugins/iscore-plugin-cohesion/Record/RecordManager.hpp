#pragma once
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QObject>

#include <QTimer>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include "RecordData.hpp"
#include <State/Address.hpp>

class DeviceExplorerModel;
namespace RedoStrategy {
struct Quiet;
}  // namespace RedoStrategy
namespace Scenario {
class ScenarioModel;
struct Point;
}  // namespace Scenario
namespace SendStrategy {
struct UndoRedo;
}  // namespace SendStrategy

using RecordCommandDispatcher = GenericMacroCommandDispatcher<
    RedoStrategy::Quiet,
    SendStrategy::UndoRedo>;

// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager final : public QObject
{
    public:
        RecordManager();

        void recordInNewBox(Scenario::ScenarioModel& scenar, Scenario::Point pt);
        // TODO : recordInExstingBox; recordFromState.
        void stopRecording();

        void commit();

    private:
        std::unique_ptr<RecordCommandDispatcher> m_dispatcher;
        ListeningState m_savedListening;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            State::Address,
            RecordData> records;
};
