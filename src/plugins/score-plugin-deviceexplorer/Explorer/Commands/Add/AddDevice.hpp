#pragma once
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

#include <score_plugin_deviceexplorer_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddDevice final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddDevice, "Add a device")
public:
  AddDevice(const DeviceDocumentPlugin& device_tree, const Device::DeviceSettings& parameters);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::DeviceSettings m_parameters;
};
}
}
