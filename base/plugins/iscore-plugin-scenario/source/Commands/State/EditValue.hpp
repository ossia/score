#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <Document/State/ItemModel/MessageItemModel.hpp>

// TODO rename file
class UpdateState : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("ScenarioControl", "UpdateState", "UpdateState")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateState)

          UpdateState(
            Path<MessageItemModel>&&,
            const iscore::MessageList& messages);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<MessageItemModel> m_path;
        MessageNodePath m_nodePath;

        MessageNode m_oldState, m_newState;

        QMap<Id<Process>, iscore::MessageList> m_previousBackup;
        QMap<Id<Process>, iscore::MessageList> m_followingBackup;
};
