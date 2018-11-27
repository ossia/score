// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageNode.hpp"

#include <Process/Process.hpp>
#include <State/Message.hpp>
#include <State/ValueConversion.hpp>

#include <score/model/tree/TreeNode.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>

#include <QStringBuilder>

#include <algorithm>

namespace Process
{


bool operator==(const StateNodeValues& lhs, const StateNodeValues& rhs) noexcept
{
  return lhs.userValue == rhs.userValue;
}

bool operator==(
    const Process::StateNodeData& lhs, const Process::StateNodeData& rhs) noexcept
{
  return lhs.name == rhs.name
         && lhs.values == rhs.values;
}

std::vector<const State::Message*> try_getNodesFromAddress(const State::MessageList& root, const State::AddressAccessor& addr)
{
  std::vector<const State::Message*> res;
  for(auto& m : root)
  {
    if(m.address.address == addr.address)
    {
      res.push_back(&m);
    }
  }
  return res;
}

}

template class SCORE_LIB_PROCESS_EXPORT TreeNode<Process::StateNodeData>;
namespace Process
{

bool StateNodeValues::empty() const noexcept
{
  return userValue.empty();
}

ossia::unit_t StateNodeValues::unit() const noexcept
{
  // Find a unit somewhere...
  for(const auto& v : userValue)
  {
    if(const auto& u = v.qualifiers.get().unit)
      return u;
  }

  return {};
}

bool StateNodeValues::hasValue() const noexcept
{
  return !userValue.empty();
}

optional<ossia::value> StateNodeValues::value() const noexcept
{
  if (!userValue.empty())
    return userValue.front().value;

  return {};
}

static bool emptyAccessors(const State::DestinationQualifiers& q) noexcept
{
  return q.get().accessors.empty();
}

ossia::value StateNodeValues::filledValue(int n) const noexcept
{
  // TODO convert according to the unit
  if (!userValue.empty() && emptyAccessors(userValue.front().qualifiers))
    return userValue.front().value;

  // Create an array
  std::vector<ossia::value> out;
  out.resize(n);

  for(auto& v : userValue)
  {
    auto& acc = v.qualifiers.get().accessors;
    if(!acc.empty() && acc[0] >= 0 && acc[0] < n)
    {
      out[acc[0]] = v.value;
    }
  }

  return out;
}


QString StateNodeData::displayName() const
{
  if(auto u = values.unit())
  {
    return name + "@[" % QString::fromStdString(ossia::get_pretty_unit_text(u)) % "]";
  }
  else
  {
    return name;
  }
}

bool StateNodeData::hasValue() const
{
  return values.hasValue();
}

optional<ossia::value> StateNodeData::value() const
{
  auto acc = ossia::get_unit_accessors(values.unit());
  if(acc.empty())
  {
    return values.value();
  }
  else
  {
    return values.filledValue(acc.size());
  }
}


State::AddressAccessor address(const Process::MessageNode& treeNode)
{
  State::AddressAccessor addr;
  addr.qualifiers = State::DestinationQualifiers{
                      ossia::destination_qualifiers{{}, treeNode.values.unit()}
  };

  const Process::MessageNode* n = &treeNode;
  while (n->parent() && n->parent()->parent())
  {
    addr.address.path.prepend(n->name);
    n = n->parent();
  }

  SCORE_ASSERT(n);
  addr.address.device = n->name;

  return addr;
}

State::Message userMessage(const Process::MessageNode& node)
{
  State::Message mess;
  mess.address = Process::address(node);

  SCORE_ASSERT(!node.values.userValue.empty());
  mess.value = node.values.userValue.front().value;

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
    ml.push_back(Process::message(node));
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
      return cld.name == node_name;
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
      if (cld.name == node_name)
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
      return cld.name == node_name;
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
      return cld.name == node_name;
    });

    return child_it != n.end() ? &*child_it : nullptr;
  }

  return node;
}


QDebug operator<<(QDebug d, const StateNodeData& mess)
{
  if (!mess.values.userValue.empty())
    d << mess.name
      << State::convert::toPrettyString(mess.values.userValue.front().value);
  else
    d << mess.name
      << "-- no user value --" ;
  return d;
}
}
