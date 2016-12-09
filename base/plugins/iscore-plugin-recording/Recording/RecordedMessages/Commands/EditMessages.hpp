#pragma once
#include <QString>
#include <Recording/RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/tools/ModelPath.hpp>
#include <iscore_plugin_recording_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace RecordedMessages
{
class ProcessModel;

class ISCORE_PLUGIN_RECORDING_EXPORT EditMessages final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      RecordedMessages::CommandFactoryName(), EditMessages, "Change messages")
public:
  EditMessages(Path<ProcessModel>&&, const RecordedMessagesList&);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<ProcessModel> m_model;
  RecordedMessagesList m_old, m_new;
};
}
