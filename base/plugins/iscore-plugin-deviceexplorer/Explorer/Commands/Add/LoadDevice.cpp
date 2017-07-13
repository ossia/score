// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <algorithm>

#include "LoadDevice.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Explorer
{
namespace Command
{
LoadDevice::LoadDevice(
    const DeviceDocumentPlugin& devplug,
    Device::Node&& node)
    : m_devicesModel{devplug},
      m_deviceNode(std::move(node))
{
}

void LoadDevice::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.removeDevice(m_deviceNode.get<Device::DeviceSettings>());
}

void LoadDevice::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.loadDevice(m_deviceNode);
}

void LoadDevice::serializeImpl(DataStreamInput& d) const
{
  d << m_devicesModel;
  d << m_deviceNode;
}

void LoadDevice::deserializeImpl(DataStreamOutput& d)
{
  d >> m_devicesModel;
  d >> m_deviceNode;
}

ReloadWholeDevice::ReloadWholeDevice(
    const DeviceDocumentPlugin& devplug,
    Device::Node&& oldNode,
    Device::Node&& newNode)
    : m_devicesModel{devplug}
    , m_oldNode(std::move(oldNode))
    , m_newNode(std::move(newNode))
{
}

void ReloadWholeDevice::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.removeDevice(m_newNode.get<Device::DeviceSettings>());
  devplug.updateProxy.loadDevice(m_oldNode);
}

void ReloadWholeDevice::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.removeDevice(m_oldNode.get<Device::DeviceSettings>());
  devplug.updateProxy.loadDevice(m_newNode);
}

void ReloadWholeDevice::serializeImpl(DataStreamInput& d) const
{
  d << m_devicesModel << m_oldNode << m_newNode;
}

void ReloadWholeDevice::deserializeImpl(DataStreamOutput& d)
{
  d >> m_devicesModel >> m_oldNode >> m_newNode;
}
}
}
