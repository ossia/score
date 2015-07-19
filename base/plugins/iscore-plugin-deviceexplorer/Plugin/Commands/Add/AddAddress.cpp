#include "AddAddress.hpp"
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace DeviceExplorer::Command;

AddAddress::AddAddress(ObjectPath &&device_tree,
                       Path nodePath,
                       DeviceExplorerModel::Insert insert,
                       const AddressSettings &addressSettings):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                commandName(),
                                description()},
    m_deviceTree{device_tree}
{
    m_addressSettings = addressSettings;

    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();

    iscore::Node* parentNode{};
    if (insert == DeviceExplorerModel::Insert::AsChild)
    {
        parentNode = nodePath.toNode(&explorer.rootNode());
    }
    else if (insert == DeviceExplorerModel::Insert::AsSibling)
    {
        parentNode =  nodePath.toNode(&explorer.rootNode())->parent();
    }
    m_parentNodePath = Path{parentNode};
    // TODO prevent add sibling on device
}

void AddAddress::undo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    iscore::Node* parentnode = m_parentNodePath.toNode(&explorer.rootNode());

    auto addr = parentnode->address();
    addr.path.append(m_addressSettings.name);

    // Remove from the device implementation
    auto dev_node = explorer.rootNode().childAt(m_parentNodePath.at(0));
    explorer.deviceModel()->list().device(
                dev_node->deviceSettings().name)
            .removeAddress(addr);

    // Remove from the device explorer
    explorer.removeNode(parentnode->childAt(m_createdNodeIndex));
}

void AddAddress::redo()
{
    auto& explorer = m_deviceTree.find<DeviceExplorerModel>();
    auto parentnode = m_parentNodePath.toNode(&explorer.rootNode());

    // Add in the device impl
    // Get the device node :
    auto dev_node = explorer.rootNode().childAt(m_parentNodePath.at(0));

    // Make a full path
    FullAddressSettings full = FullAddressSettings::make(
                                   m_addressSettings,
                                   parentnode->address());

    // Add in the device implementation
    explorer.deviceModel()->list().device(
                dev_node->deviceSettings().name)
            .addAddress(full);

    // Add in the device explorer
    iscore::Node* newNode = explorer.addAddress(
                                parentnode,
                                m_addressSettings);

    m_createdNodeIndex = parentnode->indexOfChild(newNode);
}

int AddAddress::createdNodeIndex() const
{
    return m_createdNodeIndex;
}

void AddAddress::serializeImpl(QDataStream &s) const
{
    s << m_deviceTree << m_parentNodePath << m_addressSettings << m_createdNodeIndex;
}

void AddAddress::deserializeImpl(QDataStream &s)
{
    s >> m_deviceTree >> m_parentNodePath >> m_addressSettings >> m_createdNodeIndex;
}
