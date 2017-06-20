#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <iscore/model/tree/TreePath.hpp>

#include <iscore_plugin_deviceexplorer_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddAddress final
    : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), AddAddress, "Add an address")
public:
  AddAddress(
      const DeviceDocumentPlugin& devplug,
      const Device::NodePath& nodePath,
      InsertMode insert,
      const Device::AddressSettings& addressSettings);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

  int createdNodeIndex() const;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<DeviceDocumentPlugin> m_devicesModel;
  Device::NodePath m_parentNodePath;
  Device::AddressSettings m_addressSettings;
};
}
}
