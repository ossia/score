#include "NodePath.hpp"

#include <QModelIndex>
#include "DeviceExplorer/Node/Node.hpp"
using namespace iscore;
NodePath::NodePath()
{

}

NodePath::NodePath(QModelIndex index)
{
    QModelIndex iter = index;

    while(iter.isValid())
    {
        m_path.prepend(iter.row());
        iter = iter.parent();
    }
}

NodePath::NodePath(const iscore::Node& node)
{
    // We have to take care of the root node.
    if(node.isInvisibleRoot())
        return;

    auto iter = &node;
    while(! iter->isDevice())
    {
        m_path.prepend(iter->parent()->indexOfChild(iter));
        iter = iter->parent();
    }
    m_path.prepend(iter->parent()->indexOfChild(iter));
}

Node* NodePath::toNode(Node *iter)
{
    const int pathSize = m_path.size();

    for (int i = 0; i < pathSize; ++i)
    {
        if (m_path.at(i) < iter->childCount())
        {
            iter = iter->childAt(m_path.at(i));
        }
        else
        {
            return nullptr;
        }
    }

    return iter;
}

void NodePath::append(int i)
{
    m_path.append(i);
}

void NodePath::prepend(int i)
{
    m_path.prepend(i);
}

const int &NodePath::at(int i) const
{
    return m_path.at(i);
}

int &NodePath::back()
{
    return m_path.back();
}

int NodePath::size() const
{
    return m_path.size();
}

bool NodePath::empty() const
{
    return m_path.empty();
}

void NodePath::removeLast()
{
    m_path.removeLast();
}

void NodePath::clear()
{
    m_path.clear();
}

void NodePath::reserve(int size)
{
    m_path.reserve(size);
}
