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
// TODO split this command.
class Remove final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), Remove, "Remove an Explorer node")
public:
  // For addresses
  Remove(const DeviceDocumentPlugin& devplug, Device::NodePath&& path);

  // For devices
  Remove(const DeviceDocumentPlugin& devplug, const Device::Node& node);

  ~Remove();

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

protected:
  bool m_device{};
  score::Command* m_cmd{};
};
}
}
