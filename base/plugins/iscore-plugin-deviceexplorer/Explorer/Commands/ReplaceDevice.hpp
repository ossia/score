#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
// Replaces all the nodes of a device by new nodes.
class ReplaceDevice final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), ReplaceDevice, "Replace a device")
public:
  ReplaceDevice(
      const DeviceDocumentPlugin& device_tree,
      int deviceIndex,
      Device::Node&& rootNode);
  ReplaceDevice(
      const DeviceDocumentPlugin& device_tree,
      int deviceIndex,
      Device::Node&& oldDevice,
      Device::Node&& newDevice);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<DeviceDocumentPlugin> m_deviceTree;
  int m_deviceIndex{};
  Device::Node m_deviceNode;
  Device::Node m_savedNode;
};
}
}
