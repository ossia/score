#pragma once
#include <RecordedMessages/Commands/RecordedMessagesCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
namespace RecordedMessages
{
class ProcessModel;

class EditMessages final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(RecordedMessages::CommandFactoryName(), EditMessages, "Edit a RecordedMessages script")
    public:
        EditMessages(
                Path<ProcessModel>&& model,
                const QString& text);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<ProcessModel> m_model;
        QString m_old, m_new;
};
}
