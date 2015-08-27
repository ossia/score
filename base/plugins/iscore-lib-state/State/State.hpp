#pragma once
#include <QVariant>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
#include <State/Message.hpp>
#include <State/DynamicState.hpp>

namespace iscore
{
/**
 * @brief The StateData class
 *
 * The data element that is meant to serve as the backbone of
 * trees of states.
 *
 * The data can be of two sorts:
 *  - A MessageList
 *  - A ProcessState
 *
 * Note : maybe the class should not be a variant of these two
 * but an aggregate instead ?
 * Note : ProcessState -> ProcessStateList ?
 *
 */
class StateData : public VariantBasedNode<
        iscore::MessageList,
        DynamicState>
{
        ISCORE_SERIALIZE_FRIENDS(StateData, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateData, JSONObject)

    public:
        StateData(const StateData& t) = default;
        StateData(StateData&& t) = default;
        StateData& operator=(const StateData& t) = default;
        StateData() = default;
        explicit StateData(const QString& name):
            VariantBasedNode{},
            m_name(name)
        {

        }

        template<typename T>
        StateData(const T& t, const QString& name):
            VariantBasedNode{t},
            m_name(name)
        {

        }
        template<typename T>
        StateData(T&& t, const QString& name):
            VariantBasedNode{std::move(t)},
            m_name(name)
        {

        }

        const QString& name() const
        { return m_name; }
        void setName(const QString& n)
        { m_name = n; }

    private:
        QString m_name;
};

using StateNode = TreeNode<StateData>;
using StatePath = TreePath<StateNode>;
}
