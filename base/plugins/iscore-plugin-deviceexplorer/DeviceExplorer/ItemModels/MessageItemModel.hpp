#pragma once
#include <State/StateMimeTypes.hpp>
#include <State/Message.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <DeviceExplorer/ItemModels/NodeBasedItemModel.hpp>
#include <QModelIndex>

namespace iscore
{

using OptionalValue = boost::optional<iscore::Value>;
struct StateNodeMessage
{
        QStringList addr; // device + path
        OptionalValue processValue;
        OptionalValue userValue;
};

struct StateNodeData
{
        QString name;
        OptionalValue processValue;
        OptionalValue userValue;

        StateNodeData(
                const QString& name,
                const OptionalValue& proc,
                const OptionalValue& user):
            name{name},
            processValue{proc},
            userValue{user}
        {

        }

        StateNodeData() = default;
        StateNodeData(const StateNodeData&) = default;
        StateNodeData(StateNodeData&&) = default;
        StateNodeData& operator=(const StateNodeData&) = default;
        StateNodeData& operator=(StateNodeData&&) = default;

        const QString& displayName() const
        { return name; }

        bool hasValue() const
        { return processValue || userValue; }

        iscore::Value value() const
        {
            if(processValue)
                return *processValue;
            else if(userValue)
                return *userValue;
            else
                return iscore::Value{};
        }

        QString displayValue() const
        {
            if(processValue)
                return processValue->toString();
            else if(userValue)
                return userValue->toString();
            else
                return QString{};
        }
};

using MessageNode = TreeNode<StateNodeData>;
using MessageNodePath = TreePath<MessageNode>;

/**
 * @brief The MessageItemModel class
 *
 * Used as a wrapper with trees of node_type, to represent them
 * the Qt way.
 *
 */
class MessageItemModel : public TreeNodeBasedItemModel<MessageNode>
{
        Q_OBJECT
    public:
        using node_type = TreeNodeBasedItemModel<MessageNode>::node_type;

        enum class Column : int
        {
            Name = 0,
            Value,
            Count
        };

        MessageItemModel(
                iscore::CommandStack& stack,
                QObject* parent);
        MessageItemModel& operator=(const MessageItemModel&);
        MessageItemModel& operator=(const node_type&);
        MessageItemModel& operator=(node_type&&);

        // Returns a flattened list of all the messages in the tree.
        iscore::MessageList flatten() const;

        const node_type& rootNode() const override
        { return m_rootNode; }
        node_type& rootNode() override
        { return m_rootNode; }

        // Specific operations
        void merge(const StateNodeMessage&);

        void removeFromUser(const iscore::Address&);
        void removeFromProcess(const iscore::Address&);

        // AbstractItemModel interface
        int columnCount(const QModelIndex &parent) const override;

        QVariant data(const QModelIndex &index, int role) const override;
        bool setData(const QModelIndex &index, const QVariant &value, int role) override;
        void editData(const MessageNodePath& path,
                      const OptionalValue& processValue,
                      const OptionalValue& userValue);

        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        bool setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role) override;

        QStringList mimeTypes() const override;
        QMimeData *mimeData(const QModelIndexList &indexes) const override;
        bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
        bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

        Qt::DropActions supportedDragActions() const override;
        Qt::DropActions supportedDropActions() const override;

        Qt::ItemFlags flags(const QModelIndex &index) const override;

    signals:
        void userMessage(const iscore::Message&);

    private:
        node_type m_rootNode;

        iscore::CommandStack& m_stack;
};
}
