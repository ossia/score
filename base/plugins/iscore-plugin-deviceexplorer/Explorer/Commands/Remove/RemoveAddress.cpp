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
    Path<DeviceDocumentPlugin>&& device_tree, const Device::NodePath& nodePath)
    : m_devicesModel{device_tree}, m_nodePath{nodePath}
{
  auto& devplug = m_devicesModel.find();

  auto n = nodePath.toNode(&devplug.rootNode());
  ISCORE_ASSERT(n);
  ISCORE_ASSERT(n->is<Device::AddressSettings>());
  m_savedNode = *n;
}

void RemoveAddress::undo() const
{
  auto& devplug = m_devicesModel.find();
  auto parentPath = m_nodePath;
  parentPath.removeLast();

  devplug.updateProxy.addNode(parentPath, m_savedNode, m_nodePath.back());
}

void RemoveAddress::redo() const
{
  auto& devplug = m_devicesModel.find();
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
