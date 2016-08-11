#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Recording/Record/RecordData.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>

namespace Curve
{
namespace Settings
{
class Model;
}
}
namespace Recording
{
struct RecordContext;
// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager final : public QObject
{
        Q_OBJECT
    public:
        RecordManager(RecordContext& ctx);

        void setup();
        void stop();

        void commit();

    signals:
        void requestPlay();

    private:
        void messageCallback(const State::Address& addr, const State::Value& val);
        void parameterCallback(const State::Address& addr, const State::Value& val);

        RecordContext& m_context;
        const Curve::Settings::Model& m_settings;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        QTimer m_recordTimer;
        bool m_firstValueReceived{};
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            Device::FullAddressSettings,
        RecordData> records;
};
}
