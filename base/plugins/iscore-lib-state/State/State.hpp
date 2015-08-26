#pragma once
#include <QVariant>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
#include <State/Message.hpp>
#include <State/ProcessState.hpp>

namespace iscore
{
class StateData : public VariantBasedNode<
        iscore::MessageList,
        ProcessState,
        InvisibleRootNodeTag>
{
        ISCORE_SERIALIZE_FRIENDS(StateData, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateData, JSONObject)

    public:
        StateData(const StateData& t) = default;
        StateData(StateData&& t) = default;
        StateData& operator=(const StateData& t) = default;
        StateData():
            VariantBasedNode{InvisibleRootNodeTag{}}
        {

        }

        explicit StateData(const QString& name):
            VariantBasedNode{InvisibleRootNodeTag{}},
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
