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
/**
 * @brief The RemoveAddress class
 *
 * Removes an address and its child in the device explorer.
 */
class RemoveAddress final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), RemoveAddress, "Remove an address")
public:
  RemoveAddress(
      const DeviceDocumentPlugin& devplug,
      const Device::NodePath& nodePath);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<DeviceDocumentPlugin> m_devicesModel;
  Device::NodePath m_nodePath;
  Device::Node m_savedNode;
};
}
}
