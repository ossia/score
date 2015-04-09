#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>

namespace DeviceExplorer
{
    namespace Command
    {
        class AddAddress : public iscore::SerializableCommand
        {
            ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(AddAddress, "DeviceExplorerControl")
                AddAddress(ObjectPath&& device_tree,
                                    QModelIndex index,
                                    DeviceExplorerModel::Insert insert,
                                    const QList<QString>& addressSettings);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;
                QList<int> m_parentNodePath;
                //QModelIndex m_index;
                //DeviceExplorerModel::Insert m_insert;
                QList<QString> m_addressSettings;
                int m_createdNodeIndex;

                //QModelIndex m_createdIndex;
        };
    }
}

