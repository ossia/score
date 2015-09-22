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

        void insertNode(iscore::Node& parentNode,
                        const iscore::Node& other,
                        int row);
        void removeNode(iscore::Node::const_iterator node);

        QModelIndex parent(const QModelIndex &index) const override;

        QModelIndex index(int row,
                          int column,
                          const QModelIndex &parent) const override;


        int rowCount(const QModelIndex &parent) const override;
        bool hasChildren(const QModelIndex &parent) const override;

        // TODO references.
        iscore::Node* nodeFromModelIndex(const QModelIndex &index) const;
        QModelIndex modelIndexFromNode(iscore::Node& n) const;
};
