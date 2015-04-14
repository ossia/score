
#include "Remove.hpp"

#include <iostream> //DEBUG

using namespace DeviceExplorer::Command;

const char* Remove::className() { return "Remove"; }
QString Remove::description() { return "Remove Node"; }

Remove::Remove(ObjectPath &&device_tree, Node *node):
    iscore::SerializableCommand{"DeviceExplorerControl",
                            className(),
                            description()},
    m_deviceTree{device_tree}
{
    m_node = *node;
}

void
Remove::undo()
{

}

void
Remove::redo()
{

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
