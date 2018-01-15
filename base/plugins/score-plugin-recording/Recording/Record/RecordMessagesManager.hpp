#pragma once
#include <Recording/Record/RecordProviderFactory.hpp>
#include <Recording/Record/RecordTools.hpp>
namespace Recording
{
struct RecordedMessage
{
    double percentage;
    State::Message m;
};
class MessageRecorder : public QObject,
                        public RecordProvider,
                        public Nano::Observer
{
  Q_OBJECT
public:
  RecordContext& context;

  MessageRecorder(RecordContext& ctx);

  bool setup(const Box&, const RecordListening&) override;
  void stop() override;

Q_SIGNALS:
  void firstMessageReceived();

private:
  void on_valueUpdated(const State::Address& addr, const ossia::value& val);
  std::vector<QPointer<Device::DeviceInterface>> m_recordCallbackConnections;

  Scenario::ProcessModel* m_createdProcess{};
  std::vector<RecordedMessage> m_records;
};
}
