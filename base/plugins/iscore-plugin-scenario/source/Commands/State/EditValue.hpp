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
            Path<iscore::MessageItemModel>&&,
            const iscore::MessageNodePath&,
            const QVariant&);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<iscore::MessageItemModel> m_path;
        iscore::MessageNodePath m_nodePath;

        iscore::OptionalValue m_oldProcess, m_oldUser;
        iscore::OptionalValue m_newProcess, m_newUser;
};
