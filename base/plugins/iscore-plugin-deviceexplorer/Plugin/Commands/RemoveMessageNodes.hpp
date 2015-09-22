#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <DeviceExplorer/ItemModels/MessageItemModel.hpp>


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
