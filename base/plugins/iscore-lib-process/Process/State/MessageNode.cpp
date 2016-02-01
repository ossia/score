#include "MessageNode.hpp"
#include <State/Message.hpp>
#include <iscore/tools/TreeNode.hpp>

namespace Process
{
State::Address address(const Process::MessageNode& treeNode)
{
    State::Address addr;
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

State::Message userMessage(const Process::MessageNode& node)
{
    State::Message mess;
    mess.address = Process::address(node);

    ISCORE_ASSERT(bool(node.values.userValue));
    mess.value = *node.values.userValue;

    return mess;
}

State::Message message(const Process::MessageNode& node)
{
    State::Message mess;
    mess.address = Process::address(node);

    auto val = node.value();
    ISCORE_ASSERT(bool(val));
    mess.value = *val;

    return mess;
}

QStringList toStringList(const State::Address& addr)
{
    QStringList l;
    l.append(addr.device);
    return l + addr.path;
}

// TESTME
static void flatten_rec(
        State::MessageList& ml,
        const Process::MessageNode& node)
{
    if(node.hasValue())
    {
        ml.append(Process::message(node));
    }

    for(const auto& child : node)
    {
        flatten_rec(ml, child);
    }
}


State::MessageList flatten(const Process::MessageNode& n)
{
    State::MessageList ml;
    flatten_rec(ml, n);
    return ml;
}

static void getUserMessages_rec(
        State::MessageList& ml,
        const Process::MessageNode& node)
{
    if(node.hasValue() && node.values.userValue)
    {
        ml.append(Process::userMessage(node));
    }

    for(const auto& child : node)
    {
        getUserMessages_rec(ml, child);
    }
}

State::MessageList getUserMessages(const MessageNode& n)
{
    State::MessageList ml;
    getUserMessages_rec(ml, n);
    return ml;
}

}
