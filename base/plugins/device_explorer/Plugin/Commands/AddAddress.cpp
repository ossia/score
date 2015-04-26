#include "AddAddress.hpp"

using namespace DeviceExplorer::Command;

const char* AddAddress::className() { return "AddAddress"; }
QString AddAddress::description() { return "Add an address"; }

AddAddress::AddAddress(ObjectPath &&device_tree, QModelIndex index, DeviceExplorerModel::Insert insert, const AddressSettings &addressSettings):
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
        parentNode = explorer->nodeFromModelIndex(index);
    }
    else if (insert == DeviceExplorerModel::Insert::AsSibling)
    {
        parentNode = explorer->nodeFromModelIndex(index)->parent();
    }
    m_parentNodePath = explorer->pathFromNode(*parentNode);
}

void AddAddress::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* parent = explorer->pathToNode(m_parentNodePath);
    explorer->removeNode(parent->childAt(m_createdNodeIndex));
}

void AddAddress::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* newNode = explorer->addAddress( explorer->pathToNode(m_parentNodePath), m_addressSettings);

    m_createdNodeIndex = explorer->pathToNode(m_parentNodePath)->indexOfChild(newNode);
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
