// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDataStream>
#include <QtGlobal>

#include "AddAddress.hpp"
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
AddAddress::AddAddress(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& nodePath,
    InsertMode insert,
    const Device::AddressSettings& addressSettings)
{
  m_addressSettings = addressSettings;

  const Device::Node* parentNode{};

  // DeviceExplorerWidget prevents adding a sibling on a Device
  switch (insert)
  {
    case InsertMode::AsChild:
      parentNode = nodePath.toNode(&devplug.rootNode());
      break;
    case InsertMode::AsSibling:
      parentNode = nodePath.toNode(&devplug.rootNode())->parent();
      break;
    default:
      throw std::runtime_error("AddAddress: Invalid InsertMode");
  }

  SCORE_ASSERT(parentNode);
  m_parentNodePath = Device::NodePath{*parentNode};
}

void AddAddress::undo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.removeNode(m_parentNodePath, m_addressSettings);
}

void AddAddress::redo(const score::DocumentContext& ctx) const
{
  auto& devplug = ctx.plugin<DeviceDocumentPlugin>();
  devplug.updateProxy.addAddress(m_parentNodePath, m_addressSettings, 0);
}

void AddAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_parentNodePath << m_addressSettings;
}

void AddAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_parentNodePath >> m_addressSettings;
}
}
}
