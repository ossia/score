#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <DeviceExplorer/ItemModels/MessageItemModel.hpp>

// TODO rename file.
class EditValue : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "EditValue", "EditValue")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(EditValue)

          EditValue(
            Path<iscore::MessageItemModel>&&,
            const iscore::NodePath&,
            const QVariant&);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<iscore::MessageItemModel> m_path;
        iscore::NodePath m_nodePath;

        QVariant m_old;
        QVariant m_new;
};

// TODO moveme
// TODO addme to control
class RemoveMessageNodes : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "RemoveMessageNodes", "RemoveMessageNodes")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveMessageNodes)

          RemoveMessageNodes(
            Path<iscore::MessageItemModel>&& ,
            const iscore::NodeList&);

        void undo() override;
        void redo() override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<iscore::MessageItemModel> m_path;
        QList<iscore::Node> m_savedNodes;
        QList<iscore::NodePath> m_nodePaths;
};
