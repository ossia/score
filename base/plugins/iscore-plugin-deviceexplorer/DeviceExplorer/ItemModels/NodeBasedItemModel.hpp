#pragma once
#include <QAbstractItemModel>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

/**
 * @brief The TreeNodeBasedItemModel class
 *
 * Provides basic tree-like functionality
 * shared between item models that uses the NodeType.
 */
// TESTME
// MOVEME
template<typename NodeType>
class TreeNodeBasedItemModel : public QAbstractItemModel
{
    public:
        using node_type = NodeType;

        using QAbstractItemModel::QAbstractItemModel;
        virtual NodeType& rootNode() = 0;
        virtual const NodeType& rootNode() const = 0;

        // TODO references.
        // TODO candidate for inlining
        // TODO return ptr<> ?
        NodeType* nodeFromModelIndex(const QModelIndex &index) const
        {
            auto n =  index.isValid()
                    ? static_cast<NodeType*>(index.internalPointer())
                    : const_cast<NodeType*>(&rootNode()); // TODO bleh.

            ISCORE_ASSERT(n);
            return n;
        }

        QModelIndex parent(const QModelIndex &index) const override
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

        QModelIndex index(int row,
                          int column,
                          const QModelIndex &parent) const override
        {
            if (!hasIndex(row, column, parent))
                return QModelIndex();

            auto parentItem = nodeFromModelIndex(parent);

            if (parentItem->hasChild(row))
                return createIndex(row, column, &parentItem->childAt(row));
            else
                return QModelIndex();
        }

        int rowCount(const QModelIndex &parent) const override
        {
            if(parent.column() > 0)
                return 0;

            auto parentNode = nodeFromModelIndex(parent);
            return parentNode->childCount();
        }

        bool hasChildren(const QModelIndex &parent) const override
        {
            auto parentNode = nodeFromModelIndex(parent);
            return parentNode->childCount() > 0;
        }
};
