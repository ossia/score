#pragma once
#include <State/StateMimeTypes.hpp>
#include <State/Message.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

#include <DeviceExplorer/ItemModels/NodeBasedItemModel.hpp>
#include <QModelIndex>
#include <ProcessInterface/Process.hpp>
#include <array>
namespace iscore
{

struct StateNodeMessage
{
        QStringList addr; // device + path
        OptionalValue processValue;
        OptionalValue userValue;
};

struct ProcessStateData
{
        QPointer<Process> process;
        OptionalValue value;
        int priority;
};

struct StateNodeData
{
        enum class PriorityPolicy {
            User, Previous, Following
        };

        std::array<PriorityPolicy, 3> priorities{{
            PriorityPolicy::Previous,
            PriorityPolicy::Following,
            PriorityPolicy::User
        }};

        QString name;
        QVector<ProcessStateData> previousProcessValues;
        QVector<ProcessStateData> followingProcessValues;
        OptionalValue userValue;

        StateNodeData(
                const QString& name,
                const QVector<ProcessStateData>& prev,
                const QVector<ProcessStateData>& foll,
                const OptionalValue& user):
            name{name},
            previousProcessValues{prev},
            followingProcessValues{foll},
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

        static bool hasValue(const QVector<ProcessStateData>& vec)
        {
            return std::any_of(vec.cbegin(), vec.cend(),
                            [] (const auto& pv) {
                    return bool(pv.value);
                });
        }

        bool hasValue() const
        {
            return hasValue(previousProcessValues) || hasValue(followingProcessValues) || bool(userValue);
        }

        // TODO here we have to choose a policy
        // if we have both previous and following processes ?

        static auto value(const QVector<ProcessStateData>& vec)
        {
            return std::find_if(vec.cbegin(), vec.cend(),
                            [] (const auto& pv) {
                    return bool(pv.value);
                });
        }

        iscore::Value value() const
        {
            for(const auto& prio : priorities)
            {
                switch(prio)
                {
                    case PriorityPolicy::User:
                    {
                        if(userValue)
                            return *userValue;
                        break;
                    }

                    case PriorityPolicy::Previous:
                    {
                        // TODO optimize me by computing them only once
                        auto it = value(previousProcessValues);
                        if(it != previousProcessValues.cend())
                            return *it->value;
                        break;
                    }

                    case PriorityPolicy::Following:
                    {
                        auto it = value(followingProcessValues);
                        if(it != followingProcessValues.cend())
                            return *it->value;
                        break;
                    }

                    default:
                        break;
                }
            }

            return iscore::Value{};

            /*
            switch(priorityPolicy)
            {
                case PriorityPolicy::User:
                    return userValue
                            ? *userValue
                            : ()

                case PriorityPolicy::Previous:

                case PriorityPolicy::Following:

                default:
                    break;
            }

            if(!previousProcessValues.empty())
            {
                auto it = std::find_if(
                            processValues.cbegin(),
                            processValues.cend(),
                            [] (const auto& pv) {
                    return bool(pv.value);
                });

                if(it != processValues.end())
                {
                    return *it->value;
                }
            }
            else if(userValue)
            {
                return *userValue;
            }
            else
            {
                return iscore::Value{};
            }
            */
        }

        QString displayValue() const
        {
            return value().toString();
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
