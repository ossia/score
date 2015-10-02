#pragma once
#include <State/Message.hpp>
#include <ProcessInterface/Process.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <QVector>
#include <QPointer>
#include <array>


class StateModel;
struct ProcessStateData
{
        Id<Process> process;
        iscore::OptionalValue value;
};

enum class PriorityPolicy {
    User, Previous, Following
};

struct StateNodeValues
{
        // TODO use lists or queues instead to manage the priorities
        QVector<ProcessStateData> previousProcessValues;
        QVector<ProcessStateData> followingProcessValues;
        iscore::OptionalValue userValue;


        std::array<PriorityPolicy, 3> priorities{{
            PriorityPolicy::Previous,
            PriorityPolicy::Following,
            PriorityPolicy::User
        }};

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

        static auto value(const QVector<ProcessStateData>& vec)
        {
            return std::find_if(vec.cbegin(), vec.cend(),
                            [] (const auto& pv) {
                    return bool(pv.value);
                });
        }

        // TODO here we have to choose a policy
        // if we have both previous and following processes ?
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
        }

        QString displayValue() const
        {
            return value().toString();
        }

};

struct StateNodeMessage
{
        iscore::Address addr; // device + path
        StateNodeValues values;
};

struct StateNodeData
{
        QString name;
        StateNodeValues values;

        const QString& displayName() const
        { return name; }

        bool hasValue() const
        { return values.hasValue(); }

        iscore::Value value() const
        { return values.value(); }
};

using MessageNode = TreeNode<StateNodeData>;
using MessageNodePath = TreePath<MessageNode>;

iscore::Address address(const MessageNode& treeNode);
iscore::Message message(const MessageNode& node);
QStringList toStringList(const iscore::Address& addr);
