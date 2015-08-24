#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        class AddAddress : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL2("DeviceExplorerControl", "AddAddress", "AddAddress")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddAddress)
                AddAddress(ModelPath<DeviceDocumentPlugin>&& device_tree,
                           const iscore::NodePath &nodePath,
                           DeviceExplorerModel::Insert insert,
                           const iscore::AddressSettings& addressSettings);

                virtual void undo() override;
                virtual void redo() override;

                int createdNodeIndex() const;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ModelPath<DeviceDocumentPlugin> m_devicesModel;
                iscore::NodePath m_parentNodePath;
                iscore::AddressSettings m_addressSettings;
                int m_createdNodeIndex;

        };
    }
}

