#pragma once
#include <Device/Address/AddressSettings.hpp>
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
class UpdateAddressSettings final : public score::Command
{
  SCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(),
      UpdateAddressSettings,
      "Update an address")
public:
  UpdateAddressSettings(
      const DeviceDocumentPlugin& devplug,
      const Device::NodePath& node,
      const Device::AddressSettings& parameters);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::NodePath m_node;

  Device::AddressSettings m_oldParameters;
  Device::AddressSettings m_newParameters;
};
}
}
