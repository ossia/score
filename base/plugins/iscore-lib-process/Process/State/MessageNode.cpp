#include "MessageNode.hpp"
iscore::Address address(const MessageNode& treeNode)
{
    iscore::Address addr;
    auto n = &treeNode;
    while(n->parent() && n->parent()->parent())
    {
        addr.path.prepend(n->name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    addr.device = n->name;

    return addr;
}

iscore::Message message(const MessageNode& node)
{
    iscore::Message mess;
    mess.address = address(node);

    auto val = node.value();
    ISCORE_ASSERT(bool(val));
    mess.value = *val;

    return mess;
}

QStringList toStringList(const iscore::Address& addr)
{
    QStringList l;
    l.append(addr.device);
    return l + addr.path;
}

// TESTME
static void flatten_rec(iscore::MessageList& ml, const MessageNode& node)
{
    if(node.hasValue())
    {
        ml.append(message(node));
    }

    for(const auto& child : node)
    {
        flatten_rec(ml, child);
    }
}


iscore::MessageList flatten(const MessageNode& n)
{
    iscore::MessageList ml;
    flatten_rec(ml, n);
    return ml;
}
