#include "NodeBasedItemModel.hpp"

QModelIndex NodeBasedItemModel::parent(const QModelIndex& index) const
{
    auto node = nodeFromModelIndex(index);
    auto parentNode = node->parent();

    if(!parentNode)
        return QModelIndex();

    auto grandparentNode = parentNode->parent();

    if(!grandparentNode)
        return QModelIndex();

    const int rowParent = grandparentNode->indexOfChild(parentNode);
    ISCORE_ASSERT(rowParent != -1);
    return createIndex(rowParent, 0, parentNode);
}

QModelIndex NodeBasedItemModel::index(
        int row,
        int column,
        const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const iscore::Node* parentItem = nodeFromModelIndex(parent);

    if (parentItem->hasChild(row))
        return createIndex(row, column, const_cast<iscore::Node*>(&parentItem->childAt(row)));
    else
        return QModelIndex();
}

void NodeBasedItemModel::removeNode(iscore::Node::const_iterator node)
{
    ISCORE_ASSERT(!node->is<InvisibleRootNodeTag>());

    if(node->is<iscore::AddressSettings>())
    {
        iscore::Node* parent = node->parent();
        ISCORE_ASSERT(parent != &rootNode());
        iscore::Node* grandparent = parent->parent();
        ISCORE_ASSERT(grandparent);
        int rowParent = grandparent->indexOfChild(parent);
        QModelIndex parentIndex = createIndex(rowParent, 0, parent);

        int row = parent->indexOfChild(&*node);

        beginRemoveRows(parentIndex, row, row);
        parent->removeChild(node);
        endRemoveRows();
    }
    else if(node->is<iscore::DeviceSettings>())
    {
        int row = rootNode().indexOfChild(&*node);

        beginRemoveRows(QModelIndex(), row, row);
        rootNode().removeChild(node);
        endRemoveRows();
    }
}


int NodeBasedItemModel::rowCount(
        const QModelIndex& parent) const
{
    if(parent.column() > 0)
        return 0;

    auto parentNode = nodeFromModelIndex(parent);
    return parentNode->childCount();
}

bool NodeBasedItemModel::hasChildren(const QModelIndex& parent) const
{
    auto parentNode = nodeFromModelIndex(parent);
    return parentNode->childCount() > 0;
}

// TODO candidate for inlining
// TODO return ptr<> ?
iscore::Node* NodeBasedItemModel::nodeFromModelIndex(
        const QModelIndex& index) const
{
    auto n =  index.isValid()
              ? static_cast<iscore::Node*>(index.internalPointer())
              : const_cast<iscore::Node*>(&rootNode()); // TODO bleh.

    ISCORE_ASSERT(n);
    return n;
}

