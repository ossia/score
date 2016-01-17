#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QDataStream>
#include <QtGlobal>

#include "AddAddress.hpp"
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace DeviceExplorer
{
namespace Command
{
AddAddress::AddAddress(
        Path<DeviceDocumentPlugin>&& device_tree,
        const Device::NodePath& nodePath,
        InsertMode insert,
        const Device::AddressSettings &addressSettings):
    m_devicesModel{device_tree}
{
    m_addressSettings = addressSettings;

    auto& devplug = m_devicesModel.find();

    const Device::Node* parentNode{};
    // DeviceExplorerWidget prevents adding a sibling on a Device
    if (insert == InsertMode::AsChild)
    {
        parentNode = nodePath.toNode(&devplug.rootNode());
    }
    else if (insert == InsertMode::AsSibling)
    {
        parentNode =  nodePath.toNode(&devplug.rootNode())->parent();
    }
    ISCORE_ASSERT(parentNode);
    m_parentNodePath = Device::NodePath{*parentNode};
}

void AddAddress::undo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.removeNode(m_parentNodePath,
                                   m_addressSettings);
}

void AddAddress::redo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.addAddress(m_parentNodePath,
                                   m_addressSettings,
                                   0);
}

void AddAddress::serializeImpl(DataStreamInput &s) const
{
    s << m_devicesModel
      << m_parentNodePath
      << m_addressSettings;
}

void AddAddress::deserializeImpl(DataStreamOutput &s)
{
    s >> m_devicesModel
      >> m_parentNodePath
      >> m_addressSettings;
}
}
}

