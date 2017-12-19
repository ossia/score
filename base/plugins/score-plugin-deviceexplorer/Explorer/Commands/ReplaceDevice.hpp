#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
// Replaces all the nodes of a device by new nodes.
class ReplaceDevice final : public score::Command
{
  SCORE_COMMAND_DECL(
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

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  int m_deviceIndex{};
  Device::Node m_deviceNode;
  Device::Node m_savedNode;
};
}
}
