#pragma once
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QObject>
#include <Device/Address/AddressSettings.hpp>

#include <QTimer>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <vector>
#include "RecordData.hpp"
#include <State/Address.hpp>

namespace Explorer
{
class DeviceExplorerModel;
}
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

namespace std
{
template <>
struct hash<Device::FullAddressSettings>
{
  std::size_t operator()(const Device::FullAddressSettings& k) const
  {
      return std::hash<State::Address>{}(k.address);
  }
};
}
// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager final : public QObject
{
    public:
        RecordManager(const iscore::DocumentContext& ctx);

        void recordInNewBox(Scenario::ScenarioModel& scenar, Scenario::Point pt);
        // TODO : recordInExstingBox; recordFromState.
        void stopRecording();

        void commit();

    private:
        const iscore::DocumentContext& m_ctx;
        std::unique_ptr<RecordCommandDispatcher> m_dispatcher;
        Explorer::ListeningState m_savedListening;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        Explorer::DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;
        bool m_firstValueReceived{};
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            Device::FullAddressSettings,
            RecordData> records;
};
