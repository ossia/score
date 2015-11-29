#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QList>

#include <Process/State/MessageNode.hpp>

class DataStreamInput;
class DataStreamOutput;
class MessageItemModel;

class RemoveMessageNodes final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveMessageNodes, "Remove user messages")

        public:
          RemoveMessageNodes(
            Path<MessageItemModel>&& ,
            const QList<const MessageNode*>&);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<MessageItemModel> m_path;
        MessageNode m_oldState;
        MessageNode m_newState;
};
