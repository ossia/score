#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreePath.hpp>

#include <score_plugin_deviceexplorer_export.h>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddAddress final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddAddress, "Add an address")
public:
  AddAddress(
      const DeviceDocumentPlugin& devplug,
      const Device::NodePath& nodePath,
      InsertMode insert,
      const Device::AddressSettings& addressSettings);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  int createdNodeIndex() const;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::NodePath m_parentNodePath;
  Device::AddressSettings m_addressSettings;
};

class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddWholeAddress final : public score::Command
{
  SCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddWholeAddress, "Add an address")
public:
  AddWholeAddress(const DeviceDocumentPlugin& devplug, const Device::FullAddressSettings& addr);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Device::FullAddressSettings m_addressSettings;
  int m_existsUpTo{};
};
}
}
