#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

struct DataStreamInput;
struct DataStreamOutput;

namespace Explorer
{
class DeviceDocumentPlugin;
namespace Command
{
// Note : could also be used for loading from the library
class LoadDevice final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), LoadDevice, "Load a device")
public:
  LoadDevice(Path<DeviceDocumentPlugin>&& device_tree, Device::Node&& node);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<DeviceDocumentPlugin> m_devicesModel;
  Device::Node m_deviceNode;
};

class ReloadWholeDevice final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      DeviceExplorerCommandFactoryName(), ReloadWholeDevice, "Reload a device")
public:
  ReloadWholeDevice(
      Path<DeviceDocumentPlugin>&& device_tree,
      Device::Node&& oldnode,
      Device::Node&& newnode);

  void undo() const override;
  void redo() const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<DeviceDocumentPlugin> m_devicesModel;
  Device::Node m_oldNode;
  Device::Node m_newNode;
};
}
}
