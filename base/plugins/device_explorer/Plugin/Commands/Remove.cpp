
#include "Remove.hpp"

#include <iostream> //DEBUG

using namespace DeviceExplorer::Command;

const char* Remove::className() { return "Remove"; }
QString Remove::description() { return "Remove Node"; }

Remove::Remove(ObjectPath &&device_tree, QModelIndex index):
    iscore::SerializableCommand{"DeviceExplorerControl",
                            className(),
                            description()},
    m_deviceTree{device_tree}
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node *node = explorer->nodeFromModelIndex(index);

    if (! node->isDevice())
    {
        m_parentPath = explorer->pathFromNode(*(node->parent()));
    }
    else
    {
        m_parentPath.clear();
    }
    m_addressSettings = node->addressSettings();
    m_nodeIndex = explorer->pathToNode(m_parentPath)->indexOfChild(node);
    m_node = node->clone();
}

void
Remove::undo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    explorer->addAddress(explorer->pathToNode(m_parentPath), m_node);
}

void
Remove::redo()
{
    auto explorer = m_deviceTree.find<DeviceExplorerModel>();
    Node* parent = explorer->pathToNode(m_parentPath);
    explorer->removeNode(parent->childAt(m_nodeIndex));
}

bool
Remove::mergeWith(const Command* /*other*/)
{
    return false;
}


void
Remove::serializeImpl(QDataStream& d) const
{

}

void
Remove::deserializeImpl(QDataStream& d)
{

}
