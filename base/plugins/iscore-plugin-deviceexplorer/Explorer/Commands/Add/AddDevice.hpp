#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <iscore_plugin_deviceexplorer_export.h>

class DataStreamInput;
class DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddDevice final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddDevice, "Add a device")
        public:
        AddDevice(Path<DeviceDocumentPlugin>&& device_tree,
                  const Device::DeviceSettings& parameters);


        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        Device::DeviceSettings m_parameters;
};
}
}
