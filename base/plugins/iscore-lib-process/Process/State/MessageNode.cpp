#include "MessageNode.hpp"
#include <State/Message.hpp>
#include <State/ValueConversion.hpp>

#include <iscore/tools/TreeNode.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <QStringBuilder>
#include <algorithm>

namespace Process
{

const QString&StateNodeData::displayName() const
{ return name.toString(); }

bool StateNodeData::hasValue() const
{ return values.hasValue(); }

State::OptionalValue StateNodeData::value() const
{ return values.value(); }



bool StateNodeValues::empty() const
{
  return previousProcessValues.isEmpty() && followingProcessValues.isEmpty() && !userValue;
}

bool StateNodeValues::hasValue(const QVector<ProcessStateData>& vec)
{
  return std::any_of(vec.cbegin(), vec.cend(),
                     [] (const auto& pv) {
    return bool(pv.value);
  });
}

bool StateNodeValues::hasValue() const
{
  return hasValue(previousProcessValues) || hasValue(followingProcessValues) || bool(userValue);
}

QVector<ProcessStateData>::const_iterator StateNodeValues::value(const QVector<ProcessStateData>& vec)
{
  return std::find_if(vec.cbegin(), vec.cend(),
                      [] (const auto& pv) {
    return bool(pv.value);
  });
}

State::OptionalValue StateNodeValues::value() const
{
  for(auto prio : priorities)
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
      // OPTIMIZEME  by computing them only once
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

  return {};
}

QString StateNodeValues::displayValue() const
{
  auto val = value();
  if(val)
    return State::convert::value<QString>(*val);
  return {};
}



State::AddressAccessor address(const Process::MessageNode& treeNode)
{
    State::AddressAccessor addr;
    addr.qualifiers = treeNode.name.qualifiers;

    const Process::MessageNode* n = &treeNode;
    while(n->parent() && n->parent()->parent())
    {
        addr.address.path.prepend(n->name.name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    addr.address.device = n->name.name;

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

QDebug operator<<(QDebug d, const ProcessStateData& mess)
{
  d << "{" << mess.process << State::convert::toPrettyString(*mess.value) << "}";
  return d;
}

QDebug operator<<(QDebug d, const StateNodeData& mess)
{
  if(mess.values.userValue)
    d << mess.name
      << mess.values.previousProcessValues
      << State::convert::toPrettyString(*mess.values.userValue)
      << mess.values.followingProcessValues;
  else
    d << mess.name
      << mess.values.previousProcessValues
      << "-- no user value --"
      << mess.values.followingProcessValues;
  return d;
}

}
