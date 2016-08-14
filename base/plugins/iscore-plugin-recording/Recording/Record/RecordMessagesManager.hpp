#pragma once
#include <Recording/Record/RecordTools.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <Recording/Record/RecordProviderFactory.hpp>
namespace Recording
{
struct RecordMessagesData
{
};
class MessageRecorder :
        public QObject,
        public RecordProvider
{
        Q_OBJECT
    public:
        RecordContext& context;

        MessageRecorder(RecordContext& ctx);

        bool setup(const Box&, const RecordListening&) override;
        void stop() override;

    signals:
        void firstMessageReceived();

    private:
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        RecordedMessages::ProcessModel* m_createdProcess{};
        QList<RecordedMessages::RecordedMessage> m_records;
};
}
