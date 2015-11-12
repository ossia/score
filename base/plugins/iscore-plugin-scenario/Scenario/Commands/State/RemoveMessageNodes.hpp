#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Device/Node/DeviceNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>

class RemoveMessageNodes final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), RemoveMessageNodes, "RemoveMessageNodes")

        public:
          RemoveMessageNodes(
            Path<MessageItemModel>&& ,
            const QList<const MessageNode*>&);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<MessageItemModel> m_path;
        MessageNode m_oldState;
        MessageNode m_newState;
};
