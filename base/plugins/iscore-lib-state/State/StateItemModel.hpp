#pragma once
#include <QAbstractItemModel>
#include <QModelIndex>
#include <State/State.hpp>
#include <State/StateMimeTypes.hpp>

namespace iscore
{
/**
 * @brief The StateItemModel class
 *
 * Used as a wrapper with trees of iscore::StateNode, to represent them
 * the Qt way.
 *
 */
class StateItemModel : public QAbstractItemModel
{
    private:
        StateNode m_rootNode;

    public:
        enum class Column : int
        {
            Name = 0,
            Messages,
            Count
        };

        StateItemModel();
        StateItemModel(const StateItemModel&);
        StateItemModel& operator=(const StateItemModel&);
        StateItemModel& operator=(const StateNode&);
        StateItemModel& operator=(StateNode&&);

        const StateNode& rootNode() const
        { return m_rootNode; }
        StateNode& rootNode()
        { return m_rootNode; }

        StateNode* nodeFromModelIndex(const QModelIndex& index);
        const StateNode* nodeFromModelIndex(const QModelIndex& index) const;

        void addState(StateNode* parent,
                      StateNode* node,
                      int row);

        void removeState(StateNode* node);

        void setStateData(StateNode* node, const iscore::MessageList& messages);

        // AbstractItemModel interface
        QModelIndex index(int row, int column, const QModelIndex &parent) const override;
        QModelIndex parent(const QModelIndex &index) const override;

        int rowCount(const QModelIndex &parent) const override;
        int columnCount(const QModelIndex &parent) const override;
        bool hasChildren(const QModelIndex &parent) const override;

        QVariant data(const QModelIndex &index, int role) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) override;

        QStringList mimeTypes() const override;
        QMimeData *mimeData(const QModelIndexList &indexes) const override;
        bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
        bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;

        Qt::ItemFlags flags(const QModelIndex &index) const override;
};
}
