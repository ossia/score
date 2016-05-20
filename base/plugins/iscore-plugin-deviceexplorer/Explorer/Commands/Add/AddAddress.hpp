#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <iscore/tools/TreePath.hpp>

#include <iscore_plugin_deviceexplorer_export.h>

struct DataStreamInput;
struct DataStreamOutput;


namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddAddress final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddAddress, "Add an address")
        public:
            AddAddress(Path<DeviceDocumentPlugin>&& device_tree,
                       const Device::NodePath &nodePath,
                       InsertMode insert,
                       const Device::AddressSettings& addressSettings);

        void undo() const override;
        void redo() const override;

        int createdNodeIndex() const;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        Device::NodePath m_parentNodePath;
        Device::AddressSettings m_addressSettings;

};
}
}
