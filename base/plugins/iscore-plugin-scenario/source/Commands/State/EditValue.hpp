#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <Document/State/ItemModel/MessageItemModel.hpp>

class EditValue : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "EditValue", "EditValue")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(EditValue)

          EditValue(
            Path<MessageItemModel>&&,
            const MessageNodePath&,
            const iscore::Value&);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<MessageItemModel> m_path;
        MessageNodePath m_nodePath;

        MessageNode m_oldState;
        StateNodeValues m_newValues;
};
