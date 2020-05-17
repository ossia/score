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
/**
 * @brief The RemoveAddress class
 *
 * Removes an address and its child in the device explorer.
 */
class RemoveAddress final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), RemoveAddress, "Remove an address")
public:
  RemoveAddress(const DeviceDocumentPlugin& devplug, const Device::NodePath& nodePath);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::NodePath m_nodePath;
  Device::Node m_savedNode;
};
}
}
