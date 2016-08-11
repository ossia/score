#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
namespace Recording
{
struct RecordMessagesData
{
};
class RecordMessagesManager final : public QObject
{
        Q_OBJECT
    public:
        RecordMessagesManager(RecordContext& ctx);

        void setup();
        void stop();

    signals:
        void requestPlay();

    private:
        RecordContext& m_context;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        QTimer m_recordTimer;
        bool m_firstValueReceived{};
        std::chrono::steady_clock::time_point start_time_pt;

        RecordedMessages::ProcessModel* m_createdProcess{};
        QList<RecordedMessages::RecordedMessage> m_records;
};
}
