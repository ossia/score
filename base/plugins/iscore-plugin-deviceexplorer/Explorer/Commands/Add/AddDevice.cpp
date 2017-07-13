// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "AddDevice.hpp"
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Device
{
struct DeviceSettings;
} // namespace iscore

namespace Explorer
{
namespace Command
{
AddDevice::AddDevice(
    const DeviceDocumentPlugin& device_tree,
    const Device::DeviceSettings& parameters)
    : m_devicesModel{device_tree}, m_parameters(parameters)
{
}

void AddDevice::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.removeDevice(m_parameters);
}

void AddDevice::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.addDevice(m_parameters);
}

void AddDevice::serializeImpl(DataStreamInput& d) const
{
  d << m_devicesModel;
  d << m_parameters;
}

void AddDevice::deserializeImpl(DataStreamOutput& d)
{
  d >> m_devicesModel;
  d >> m_parameters;
}
}
}
