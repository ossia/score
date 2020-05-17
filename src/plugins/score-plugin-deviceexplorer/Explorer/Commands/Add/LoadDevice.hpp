#pragma once
#include <Device/Node/DeviceNode.hpp>
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
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT LoadDevice final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), LoadDevice, "Load a device")
public:
  LoadDevice(const DeviceDocumentPlugin& devplug, Device::Node&& node);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::Node m_deviceNode;
};

class ReloadWholeDevice final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), ReloadWholeDevice, "Reload a device")
public:
  ReloadWholeDevice(
      const DeviceDocumentPlugin& devplug,
      Device::Node&& oldnode,
      Device::Node&& newnode);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::Node m_oldNode;
  Device::Node m_newNode;
};
}
}
