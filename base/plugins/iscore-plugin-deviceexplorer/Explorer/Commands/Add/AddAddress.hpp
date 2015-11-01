#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Device/Node/DeviceNode.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        class AddAddress final : public iscore::SerializableCommand
        {
            ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddAddress, "AddAddress")
            public:
                AddAddress(Path<DeviceDocumentPlugin>&& device_tree,
                           const iscore::NodePath &nodePath,
                           InsertMode insert,
                           const iscore::AddressSettings& addressSettings);

                void undo() const override;
                void redo() const override;

                int createdNodeIndex() const;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<DeviceDocumentPlugin> m_devicesModel;
                iscore::NodePath m_parentNodePath;
                iscore::AddressSettings m_addressSettings;

        };
    }
}
