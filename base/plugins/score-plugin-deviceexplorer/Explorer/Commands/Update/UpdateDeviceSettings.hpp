#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <QString>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class UpdateDeviceSettings final : public score::Command
{
  SCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(),
      UpdateDeviceSettings,
      "Update a device")
public:
  UpdateDeviceSettings(
      const DeviceDocumentPlugin& devplug,
      const QString& name,
      const Device::DeviceSettings& parameters);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::DeviceSettings m_oldParameters;
  Device::DeviceSettings m_newParameters;
};
}
}
