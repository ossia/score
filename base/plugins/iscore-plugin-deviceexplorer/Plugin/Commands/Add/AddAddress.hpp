#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorer/NodePath.hpp"

namespace DeviceExplorer
{
    namespace Command
    {
        class AddAddress : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("AddAddress", "AddAddress")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddAddress, "DeviceExplorerControl")
                AddAddress(ObjectPath&& device_tree,
                           NodePath nodePath,
                           DeviceExplorerModel::Insert insert,
                           const iscore::AddressSettings& addressSettings);

                virtual void undo() override;
                virtual void redo() override;

                int createdNodeIndex() const;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;
                NodePath m_parentNodePath;
                iscore::AddressSettings m_addressSettings;
                int m_createdNodeIndex;

        };
    }
}

