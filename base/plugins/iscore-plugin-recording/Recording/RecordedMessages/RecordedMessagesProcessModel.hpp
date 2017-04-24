#pragma once
#include <Process/Process.hpp>
#include <QByteArray>
#include <QString>
#include <Recording/RecordedMessages/RecordedMessagesProcessMetadata.hpp>
#include <memory>

#include <Process/TimeValue.hpp>
#include <State/Message.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_recording_export.h>
namespace RecordedMessages
{
struct RecordedMessage
{
  double percentage;
  State::Message message;
};
using RecordedMessagesList = QList<RecordedMessage>;

class ISCORE_PLUGIN_RECORDING_EXPORT ProcessModel final
    : public Process::ProcessModel
{
  ISCORE_SERIALIZE_FRIENDS
  MODEL_METADATA_IMPL(RecordedMessages::ProcessModel)
  Q_OBJECT
public:
  explicit ProcessModel(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  explicit ProcessModel(
      const ProcessModel& source,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  explicit ProcessModel(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  virtual ~ProcessModel();
  void setMessages(const QList<RecordedMessage>& script);
  const auto& messages() const
  {
    return m_messages;
  }

  // Process interface
  void setDurationAndScale(const TimeVal& newDuration) override;
  void setDurationAndGrow(const TimeVal& newDuration) override;
  void setDurationAndShrink(const TimeVal& newDuration) override;

signals:
  void messagesChanged();

protected:
private:
  RecordedMessagesList m_messages;
};
}
