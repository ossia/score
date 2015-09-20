#pragma once
#include <State/StateMimeTypes.hpp>
#include <State/Message.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <DeviceExplorer/ItemModels/NodeBasedItemModel.hpp>
#include <QModelIndex>

namespace iscore
{
/**
 * @brief The MessageItemModel class
 *
 * Used as a wrapper with trees of iscore::Node, to represent them
 * the Qt way.
 *
 */
class MessageItemModel : public NodeBasedItemModel
{
    public:
        enum class Column : int
        {
            Name = 0,
            Value,
            Count
        };

        MessageItemModel(QObject* parent);
        MessageItemModel& operator=(const MessageItemModel&);
        MessageItemModel& operator=(const iscore::Node&);
        MessageItemModel& operator=(iscore::Node&&);

        const iscore::Node& rootNode() const override
        { return m_rootNode; }
        iscore::Node& rootNode() override
        { return m_rootNode; }

        void setCommandStack(ptr<iscore::CommandStack> stk);

        template<typename... Args>
        void emplaceMessage(iscore::Node *parent, int row, Args&&... args)
        {
            ISCORE_ASSERT(parent);

            if (row == -1)
            {
                row = parent->childCount(); //insert as last child
            }

            int rowParent = 0;
            if(parent != &m_rootNode)
            {
                auto grandparent = parent->parent();
                ISCORE_ASSERT(grandparent);

                rowParent = grandparent->indexOfChild(parent);
            }

            QModelIndex parentIndex = createIndex(rowParent, 0, parent);

            beginInsertRows(parentIndex, row, row);
            parent->emplace(parent->begin() + row, std::forward<Args>(args)...);
            endInsertRows();
        }


        void removeMessage(iscore::Node* node);

        void mergeMessages(const iscore::MessageList& messages);

        // AbstractItemModel interface
        int columnCount(const QModelIndex &parent) const override;

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

    private:
        iscore::Node m_rootNode;
        ptr<iscore::CommandStack> m_stack;
};
}
