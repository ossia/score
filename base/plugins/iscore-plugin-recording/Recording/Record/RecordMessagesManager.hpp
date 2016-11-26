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
        public RecordProvider,
        public Nano::Observer
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
        void on_valueUpdated(const State::Address& addr, const ossia::value& val);
        std::vector<QPointer<Device::DeviceInterface>> m_recordCallbackConnections;

        RecordedMessages::ProcessModel* m_createdProcess{};
        QList<RecordedMessages::RecordedMessage> m_records;
};
}
