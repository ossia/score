#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDataStream>
#include <QtGlobal>

#include "AddAddress.hpp"
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
AddAddress::AddAddress(
    const DeviceDocumentPlugin& devplug,
    const Device::NodePath& nodePath,
    InsertMode insert,
    const Device::AddressSettings& addressSettings)
    : m_devicesModel{devplug}
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

  ISCORE_ASSERT(parentNode);
  m_parentNodePath = Device::NodePath{*parentNode};
}

void AddAddress::undo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.removeNode(m_parentNodePath, m_addressSettings);
}

void AddAddress::redo(const iscore::DocumentContext& ctx) const
{
  auto& devplug = m_devicesModel.find(ctx);
  devplug.updateProxy.addAddress(m_parentNodePath, m_addressSettings, 0);
}

void AddAddress::serializeImpl(DataStreamInput& s) const
{
  s << m_devicesModel << m_parentNodePath << m_addressSettings;
}

void AddAddress::deserializeImpl(DataStreamOutput& s)
{
  s >> m_devicesModel >> m_parentNodePath >> m_addressSettings;
}
}
}
