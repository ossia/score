#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class DeviceDocumentPlugin;

namespace DeviceExplorer
{
namespace Command
{
class UpdateAddressSettings final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateAddressSettings, "Update an address")
        public:
          UpdateAddressSettings(
            Path<DeviceDocumentPlugin>&& device_tree,
            const iscore::NodePath &node,
            const iscore::AddressSettings& parameters);


        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;

        iscore::NodePath m_node;

        iscore::AddressSettings m_oldParameters;
        iscore::AddressSettings m_newParameters;
};
}
}
