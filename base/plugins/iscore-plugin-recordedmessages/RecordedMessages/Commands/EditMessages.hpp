#pragma once
#include <RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <QString>

#include <iscore/tools/ModelPath.hpp>
#include <iscore_plugin_recordedmessages_export.h>
class DataStreamInput;
class DataStreamOutput;
namespace RecordedMessages
{
class ProcessModel;

class ISCORE_PLUGIN_RECORDEDMESSAGES_EXPORT EditMessages final :
        public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(RecordedMessages::CommandFactoryName(), EditMessages, "Change messages")
    public:
        EditMessages(
                Path<ProcessModel>&&,
                const RecordedMessagesList&);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<ProcessModel> m_model;
        RecordedMessagesList m_old, m_new;
};
}
