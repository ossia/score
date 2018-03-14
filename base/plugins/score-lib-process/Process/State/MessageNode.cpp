// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageNode.hpp"
#include <State/Message.hpp>
#include <State/ValueConversion.hpp>

#include <score/serialization/JSONVisitor.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <Process/Process.hpp>
#include <QStringBuilder>
#include <algorithm>
namespace Process
{

bool operator==(const Process::ProcessStateData& lhs, const Process::ProcessStateData& rhs)
{
  return
      lhs.process == rhs.process && lhs.value == rhs.value
      ;
}

bool operator==(const Process::StateNodeData& lhs, const Process::StateNodeData& rhs)
{
  return
      lhs.name.name == rhs.name.name &&
      lhs.name.qualifiers == rhs.name.qualifiers &&
      lhs.values.previousProcessValues == rhs.values.previousProcessValues &&
      lhs.values.followingProcessValues == rhs.values.followingProcessValues &&
      lhs.values.priorities == rhs.values.priorities &&
      lhs.values.userValue == rhs.values.userValue
      ;
}

}
template class SCORE_LIB_PROCESS_EXPORT TreeNode<Process::StateNodeData>;
namespace Process
{

QString StateNodeData::displayName() const
{
  return name.toString();
}

bool StateNodeData::hasValue() const
{
  return values.hasValue();
}

State::OptionalValue StateNodeData::value() const
{
  return values.value();
}

bool StateNodeValues::empty() const
{
  return previousProcessValues.isEmpty() && followingProcessValues.isEmpty()
         && !userValue;
}

bool StateNodeValues::hasValue(const QVector<ProcessStateData>& vec)
{
  return std::any_of(
      vec.cbegin(), vec.cend(), [](const auto& pv) { return bool(pv.value); });
}

bool StateNodeValues::hasValue() const
{
  return hasValue(previousProcessValues) || hasValue(followingProcessValues)
         || bool(userValue);
}

QVector<ProcessStateData>::const_iterator
StateNodeValues::value(const QVector<ProcessStateData>& vec)
{
  return std::find_if(
      vec.cbegin(), vec.cend(), [](const auto& pv) { return bool(pv.value); });
}

State::OptionalValue StateNodeValues::value() const
{
  for (auto prio : priorities)
  {
    switch (prio)
    {
      case PriorityPolicy::User:
      {
        if (userValue)
          return *userValue;
        break;
      }

      case PriorityPolicy::Previous:
      {
        // OPTIMIZEME  by computing them only once
        auto it = value(previousProcessValues);
        if (it != previousProcessValues.cend())
          return *it->value;
        break;
      }

      case PriorityPolicy::Following:
      {
        auto it = value(followingProcessValues);
        if (it != followingProcessValues.cend())
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
  if (val)
    return State::convert::value<QString>(*val);
  return {};
}

State::AddressAccessor address(const Process::MessageNode& treeNode)
{
  State::AddressAccessor addr;
  addr.qualifiers = treeNode.name.qualifiers;

  const Process::MessageNode* n = &treeNode;
  while (n->parent() && n->parent()->parent())
  {
    addr.address.path.prepend(n->name.name);
    n = n->parent();
  }

  SCORE_ASSERT(n);
  addr.address.device = n->name.name;

  return addr;
}

State::Message userMessage(const Process::MessageNode& node)
{
  State::Message mess;
  mess.address = Process::address(node);

  SCORE_ASSERT(bool(node.values.userValue));
  mess.value = *node.values.userValue;

  return mess;
}

State::Message message(const Process::MessageNode& node)
{
  State::Message mess;
  mess.address = Process::address(node);

  auto val = node.value();
  SCORE_ASSERT(bool(val));
  mess.value = *val;

  return mess;
}

// TESTME
static void
flatten_rec(State::MessageList& ml, const Process::MessageNode& node)
{
  if (node.hasValue())
  {
    ml.append(Process::message(node));
  }

  for (const auto& child : node)
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

static void
getUserMessages_rec(State::MessageList& ml, const Process::MessageNode& node)
{
  if (node.hasValue() && node.values.userValue)
  {
    ml.append(Process::userMessage(node));
  }

  for (const auto& child : node)
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

std::vector<Process::MessageNode*> try_getNodesFromAddress(
    Process::MessageNode& root, const State::AddressAccessor& addr)
{
  std::vector<Process::MessageNode*> vec;
  if (addr.address.device.isEmpty())
    return vec;

  // Find first node
  auto first_node_it = ossia::find_if(root, [&](const auto& cld) {
    return cld.displayName() == addr.address.device;
  });
  if (first_node_it == root.end())
    return vec;

  Process::MessageNode* node = &*first_node_it;

  // The n-1 first elements are just checked against the name
  const int n = addr.address.path.size();
  for (int i = 0; i < n - 1; i++)
  {
    const QString& node_name{addr.address.path[i]};

    auto& nd = *node;
    auto child_it = ossia::find_if(nd, [&](const Process::MessageNode& cld) {
      return cld.name.name == node_name;
    });

    if (child_it != nd.end())
    {
      node = &*child_it;
    }
    else
    {
      return vec;
    }
  }

  // We return all the elements that match, without caring about the
  // qualifiers.
  {
    const QString& node_name{addr.address.path.back()};
    auto& n = *node;
    for (Process::MessageNode& cld : n)
    {
      if (cld.name.name == node_name)
      {
        vec.push_back(&cld);
      }
    }
  }

  return vec;
}

Process::MessageNode* try_getNodeFromAddress(
    Process::MessageNode& root, const State::AddressAccessor& addr)
{
  if (addr.address.device.isEmpty())
    return nullptr;

  // Find first node
  auto first_node_it = ossia::find_if(root, [&](const auto& cld) {
    return cld.displayName() == addr.address.device;
  });
  if (first_node_it == root.end())
    return nullptr;

  Process::MessageNode* node = &*first_node_it;

  // The n-1 first elements are just checked against the name
  const int n = addr.address.path.size();
  for (int i = 0; i < n - 1; i++)
  {
    const QString& node_name{addr.address.path[i]};

    auto& nd = *node;
    auto child_it = ossia::find_if(nd, [&](const Process::MessageNode& cld) {
      return cld.name.name == node_name;
    });

    if (child_it != nd.end())
    {
      node = &*child_it;
    }
    else
    {
      return nullptr;
    }
  }

  // The last element is checked also against the qualifiers
  {
    const QString& node_name{addr.address.path[n - 1]};

    auto& n = *node;
    auto child_it = ossia::find_if(n, [&](const Process::MessageNode& cld) {
      return cld.name.name == node_name
             && cld.name.qualifiers == addr.qualifiers;
    });

    return child_it != n.end() ? &*child_it : nullptr;
  }

  return node;
}

QDebug operator<<(QDebug d, const ProcessStateData& mess)
{
  d << "{" << mess.process << State::convert::toPrettyString(*mess.value)
    << "}";
  return d;
}

QDebug operator<<(QDebug d, const StateNodeData& mess)
{
  if (mess.values.userValue)
    d << mess.name << mess.values.previousProcessValues
      << State::convert::toPrettyString(*mess.values.userValue)
      << mess.values.followingProcessValues;
  else
    d << mess.name << mess.values.previousProcessValues
      << "-- no user value --" << mess.values.followingProcessValues;
  return d;
}
}
