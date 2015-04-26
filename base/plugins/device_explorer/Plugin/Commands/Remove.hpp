#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>

namespace DeviceExplorer
{
    namespace Command
    {

        class Remove : public iscore::SerializableCommand
        {
            ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(Remove, "DeviceExplorerControl")

                Remove(ObjectPath&& device_tree, QModelIndex index);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                ObjectPath m_deviceTree;
                DeviceExplorerModel::Path m_parentPath;
                AddressSettings m_addressSettings;
                int m_nodeIndex{};

                //QByteArray m_serializedNode;
                Node* m_node{};
        };
    }
}
