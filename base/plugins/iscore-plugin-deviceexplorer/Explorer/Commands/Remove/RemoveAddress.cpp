// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDataStream>
#include <QtGlobal>
#include <algorithm>

#include "RemoveAddress.hpp"
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/tree/TreeNode.hpp>
#include <iscore/model/tree/TreePath.hpp>

namespace Explorer
{
namespace Command
{
RemoveAddress::RemoveAddress(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& nodePath)
    : m_devicesModel{devplug}, m_nodePath{nodePath}
{
  auto n = nodePath.toNode(&devplug.rootNode());
  ISCORE_ASSERT(n);
  ISCORE_ASSERT(n->is<Device::AddressSettings>());
  m_savedNode = *n;
}

void RemoveAddress::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  auto parentPath = m_nodePath;
  parentPath.removeLast();

  devplug.updateProxy.addNode(parentPath, m_savedNode, m_nodePath.back());
}

void RemoveAddress::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  auto parentPath = m_nodePath;
  parentPath.removeLast();
  devplug.updateProxy.removeNode(
      parentPath, m_savedNode.get<Device::AddressSettings>());
}

void RemoveAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_devicesModel << m_nodePath << m_savedNode;
}

void RemoveAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_devicesModel >> m_nodePath >> m_savedNode;
}
}
}
