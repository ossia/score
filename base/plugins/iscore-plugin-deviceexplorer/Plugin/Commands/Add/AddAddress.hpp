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
            ISCORE_COMMAND
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddAddress, "DeviceExplorerControl")
                AddAddress(ObjectPath&& device_tree,
                           Path nodePath,
                           DeviceExplorerModel::Insert insert,
                           const AddressSettings& addressSettings);

                virtual void undo() override;
                virtual void redo() override;

                int createdNodeIndex() const;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;
                Path m_parentNodePath;
                AddressSettings m_addressSettings;
                int m_createdNodeIndex;

        };
    }
}

