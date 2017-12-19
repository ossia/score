// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "UpdateAddressSettings.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/model/tree/TreeNode.hpp>

namespace Explorer
{
namespace Command
{
UpdateAddressSettings::UpdateAddressSettings(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& node,
    const Device::AddressSettings& parameters)
    :m_node(node), m_newParameters(parameters)
{
  auto n = m_node.toNode(&devplug.rootNode());
  SCORE_ASSERT(n);

  m_oldParameters = n->get<Device::AddressSettings>();
}

void UpdateAddressSettings::undo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.updateAddress(m_node, m_oldParameters);
}

void UpdateAddressSettings::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.updateAddress(m_node, m_newParameters);
}

void UpdateAddressSettings::serializeImpl(DataStreamInput& d) const
{
  d << m_node << m_oldParameters << m_newParameters;
}

void UpdateAddressSettings::deserializeImpl(DataStreamOutput& d)
{
  d >> m_node >> m_oldParameters >> m_newParameters;
}
}
}
