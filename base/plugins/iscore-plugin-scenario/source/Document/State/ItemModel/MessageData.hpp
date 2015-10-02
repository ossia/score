#pragma once
#include <State/Message.hpp>

#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
namespace iscore
{
class MessageData : public VariantBasedNode<
        iscore::Message>
{
//        ISCORE_SERIALIZE_FRIENDS(StateData, DataStream)
//        ISCORE_SERIALIZE_FRIENDS(StateData, JSONObject)

    public:
        using VariantBasedNode<iscore::Message>::VariantBasedNode;

        QString name() const
        {
            if(is<iscore::Message>())
            {
                return get<iscore::Message>().address.path.last();
            }

            return {};
        }

        QString value() const
        {
            if(is<iscore::Message>())
            {
                return get<iscore::Message>().value.toString();
            }

            return {};
        }
};

using MessageNode = TreeNode<MessageData>;
using MessagePath = TreePath<MessageNode>;
}
