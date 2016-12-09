#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <QString>
#include <iscore/command/Command.hpp>
#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class UpdateDeviceSettings final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(),
      UpdateDeviceSettings,
      "Update a device")
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
