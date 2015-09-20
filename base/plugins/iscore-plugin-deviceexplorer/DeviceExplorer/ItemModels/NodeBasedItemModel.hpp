#pragma once
#include <QAbstractItemModel>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

/**
 * @brief The NodeBasedItemModel class
 *
 * Provides basic tree-like functionality
 * shared between item models that uses the iscore::Node.
 */
class NodeBasedItemModel : public QAbstractItemModel
{
    public:
        using QAbstractItemModel::QAbstractItemModel;
        virtual iscore::Node& rootNode() = 0;
        virtual const iscore::Node& rootNode() const = 0;

        QModelIndex parent(const QModelIndex &index) const override;

        QModelIndex index(int row,
                          int column,
                          const QModelIndex &parent) const override;

        void removeNode(iscore::Node::const_iterator node);

        int rowCount(const QModelIndex &parent) const override;
        bool hasChildren(const QModelIndex &parent) const;

        // TODO references.
        iscore::Node* nodeFromModelIndex(const QModelIndex &index) const;
};
