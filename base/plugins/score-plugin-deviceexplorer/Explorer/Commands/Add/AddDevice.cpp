// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "AddDevice.hpp"
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Device
{
struct DeviceSettings;
} // namespace score

namespace Explorer
{
namespace Command
{
AddDevice::AddDevice(
    const DeviceDocumentPlugin& device_tree,
    const Device::DeviceSettings& parameters)
    : m_parameters(parameters)
{
}

void AddDevice::undo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.removeDevice(m_parameters);
}

void AddDevice::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.addDevice(m_parameters);
}

void AddDevice::serializeImpl(DataStreamInput& d) const
{
  d << m_parameters;
}

void AddDevice::deserializeImpl(DataStreamOutput& d)
{
  d >> m_parameters;
}
}
}
