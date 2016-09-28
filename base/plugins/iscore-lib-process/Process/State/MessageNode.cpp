#include "MessageNode.hpp"
#include <State/Message.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <QStringBuilder>
namespace Process
{
State::AddressAccessor address(const Process::MessageNode& treeNode)
{
    State::AddressAccessor addr;
    auto n = &treeNode;
    while(n->parent() && n->parent()->parent())
    {
        addr.address.path.prepend(n->name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    addr.address.device = n->name;

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

QStringList toStringList(const State::AddressAccessor& addr)
{
    QStringList l;
    l += addr.address.device;
    l += addr.address.path;
    l.back() += addr.accessorsString();
    return l;
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


Process::MessageNode* try_getNodeFromAddress(
        Process::MessageNode& root,
        const State::Address& addr)
{
    if(addr.device.isEmpty())
        return nullptr;

    // Find first node
    auto first_node_it = ossia::find_if(root, [&] (const auto& cld) {
        return cld.displayName() == addr.device;
    });
    if(first_node_it == root.end())
        return nullptr;

    Process::MessageNode* node = &*first_node_it;

    for(auto& node_name : addr.path)
    {
        auto& n = *node;
        auto child_it = ossia::find_if(n, [&] (const auto& cld) {
            return cld.displayName() == node_name;
        } );
        if(child_it != n.end())
        {
            node = &*child_it;
        }
        else
        {
            return nullptr;
        }
    }

    return node;
}

}
