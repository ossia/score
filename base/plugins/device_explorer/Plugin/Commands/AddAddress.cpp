#include "AddAddress.hpp"

using namespace DeviceExplorer::Command;

const char* AddAddress::className() { return "AddAddress"; }
QString AddAddress::description() { return "Add an address"; }

AddAddress::AddAddress(ObjectPath &&device_tree, Path nodePath, DeviceExplorerModel::Insert insert, const AddressSettings &addressSettings):
    iscore::SerializableCommand{"DeviceExplorerControl",
                                className(),
                                description()},
    m_deviceTree{device_tree}
{
    m_addressSettings = addressSettings;

    auto explorer = m_deviceTree.find<DeviceExplorerModel>();

    Node* parentNode{};
    if (insert == DeviceExplorerModel::Insert::AsChild)
    {
        parentNode =  nodePath.toNode(explorer->rootNode());
    }
    else if (insert == DeviceExplorerModel::Insert::AsSibling)
    {
        parentNode =  nodePath.toNode(explorer->rootNode())->parent();
    }
    m_parentNodePath = Path{parentNode};
}

void AddAddress::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* parent = m_parentNodePath.toNode(explorer->rootNode());
    explorer->removeNode(parent->childAt(m_createdNodeIndex));
}

void AddAddress::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* newNode = explorer->addAddress( m_parentNodePath.toNode(explorer->rootNode()), m_addressSettings);

    m_createdNodeIndex = m_parentNodePath.toNode(explorer->rootNode())->indexOfChild(newNode);
}

bool AddAddress::mergeWith(const iscore::Command *other)
{
    return false;
}

void AddAddress::serializeImpl(QDataStream &s) const
{

}

void AddAddress::deserializeImpl(QDataStream &s )
{

}
