#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QString>

class DataStreamInput;
class DataStreamOutput;

namespace DeviceExplorer
{
class DeviceDocumentPlugin;
namespace Command
{
class UpdateDeviceSettings final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateDeviceSettings, "Update a device")
        public:
        UpdateDeviceSettings(
                  Path<DeviceDocumentPlugin>&& device_tree,
                  const QString& name,
                  const Device::DeviceSettings& parameters);


        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        Device::DeviceSettings m_oldParameters;
        Device::DeviceSettings m_newParameters;
};
}
}
