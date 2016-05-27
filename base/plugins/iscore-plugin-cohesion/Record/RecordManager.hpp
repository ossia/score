#pragma once
#include <Record/RecordTools.hpp>
#include <Record/RecordData.hpp>

namespace Curve
{
namespace Settings
{
class Model;
}
}
namespace Recording
{
// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager final : public QObject
{
        Q_OBJECT
    public:
        RecordManager(const iscore::DocumentContext& ctx);

        void recordInNewBox(const Scenario::ScenarioModel& scenar, Scenario::Point pt);
        // TODO : recordInExstingBox; recordFromState.
        void stopRecording();

        void commit();

    signals:
        void requestPlay();

    private:
        void messageCallback(const State::Address& addr, const State::Value& val);
        void parameterCallback(const State::Address& addr, const State::Value& val);

        const iscore::DocumentContext& m_ctx;
        const Curve::Settings::Model& m_settings;
        std::unique_ptr<RecordCommandDispatcher> m_dispatcher;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        Explorer::DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;
        bool m_firstValueReceived{};
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            Device::FullAddressSettings,
            RecordData> records;
};
}
