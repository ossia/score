#include "AddAddress.hpp"
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace DeviceExplorer::Command;

AddAddress::AddAddress(
        Path<DeviceDocumentPlugin>&& device_tree,
        const iscore::NodePath& nodePath,
        InsertMode insert,
        const iscore::AddressSettings &addressSettings):
    iscore::SerializableCommand{factoryName(),
                                commandName(),
                                description()},
    m_devicesModel{device_tree}
{
    m_addressSettings = addressSettings;

    auto& devplug = m_devicesModel.find();

    const iscore::Node* parentNode{};
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
    m_parentNodePath = iscore::NodePath{*parentNode};
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

void AddAddress::serializeImpl(QDataStream &s) const
{
    s << m_devicesModel
      << m_parentNodePath
      << m_addressSettings;
}

void AddAddress::deserializeImpl(QDataStream &s)
{
    s >> m_devicesModel
      >> m_parentNodePath
      >> m_addressSettings;
}
