// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "UpdateAddressSettings.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>

namespace Explorer
{
namespace Command
{
UpdateAddressSettings::UpdateAddressSettings(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& node,
    const Device::AddressSettings& parameters)
    : m_devicesModel{devplug}, m_node(node), m_newParameters(parameters)
{
  auto n = m_node.toNode(&devplug.rootNode());
  ISCORE_ASSERT(n);

  m_oldParameters = n->get<Device::AddressSettings>();
}

void UpdateAddressSettings::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.updateAddress(m_node, m_oldParameters);
}

void UpdateAddressSettings::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.updateAddress(m_node, m_newParameters);
}

void UpdateAddressSettings::serializeImpl(DataStreamInput& d) const
{
  d << m_devicesModel << m_node << m_oldParameters << m_newParameters;
}

void UpdateAddressSettings::deserializeImpl(DataStreamOutput& d)
{
  d >> m_devicesModel >> m_node >> m_oldParameters >> m_newParameters;
}
}
}
