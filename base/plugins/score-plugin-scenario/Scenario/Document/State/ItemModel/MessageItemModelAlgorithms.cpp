// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "MessageItemModelAlgorithms.hpp"

#include <Device/Node/DeviceNode.hpp>
#include <Process/State/MessageNode.hpp>
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/editor/state/destination_qualifiers.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QList>
#include <QString>
#include <QStringList>
#include <QTypeInfo>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <set>
#include <vector>
namespace Process
{
static QStringList toStringList(const State::AddressAccessor& addr)
{
  QStringList l;
  l += addr.address.device;
  l += addr.address.path;
  return l;
}
}

namespace Scenario
{
static
Process::MessageNode* try_getNodeFromString_impl(
    Process::MessageNode& n
    , QStringList::const_iterator begin
    , QStringList::const_iterator end)
{
  if(begin == end)
  {
    return &n;
  }
  else
  {
    auto next = begin + 1;
    if(next == end)
    {
      for (auto& child : n)
      {
        if (child.name == *begin)
        {
          return &child;
        }
      }
    }
    else
    {
      for (auto& child : n)
      {
        if (child.name == *begin)
        {
          return try_getNodeFromString_impl(child, next, end);
        }
      }
    }
  }
  return nullptr;
}

static
Process::MessageNode* try_getNodeFromString(Process::MessageNode& n, const State::AddressAccessor& addr)
{
  for (auto& child : n)
  {
    if (child.displayName() == addr.address.device)
    {
      return try_getNodeFromString_impl(child, addr.address.path.begin(), addr.address.path.end());
    }
  }
  return nullptr;
}

static bool removable(const Process::MessageNode& node)
{
  return node.values.empty() && !node.hasChildren();
}

static void cleanupNode(Process::MessageNode& rootNode)
{
  for (auto it = rootNode.begin(); it != rootNode.end();)
  {
    auto& child = *it;
    if (removable(child))
    {
      it = rootNode.erase(it);
    }
    else
    {
      it++;
    }
  }
}

static bool match(Process::MessageNode& node, const State::Message& mess)
{
  Process::MessageNode* n = &node;

  QStringList path = Process::toStringList(mess.address);
  std::reverse(path.begin(), path.end());
  int i = 0;
  int imax = path.size();
  while (n->parent() && i < imax)
  {
    if (n->name == path.at(i))
    {
      if (i == imax - 1 && !n->parent()->parent())
      {
        return true;
      }

      i++;
      n = n->parent();
      continue;
    }

    return false;
  }

  return false;
}

static void rec_delete(Process::MessageNode& node)
{
  if (!node.hasChildren())
  {
    // kthxbai
    auto parent = node.parent();
    if (parent)
    {
      auto it = std::find_if(
          parent->begin(), parent->end(),
          [&](const auto& other) { return &node == &other; });
      if (it != parent->end())
      {
        parent->erase(it);
        rec_delete(*parent);
      }
    }
  }
}

static bool match(
    const QString& cur_node,
    const State::AddressAccessor& mess, int i)
{
  if (i == 0)
  {
    return mess.address.device == cur_node;
  }
  else if (i < mess.address.path.size())
  {
    return mess.address.path[i - 1] == cur_node;
  }
  else
  {
    return mess.address.path.back() == cur_node;
  }
}

static QString
get_at(const State::AddressAccessor& mess, int i)
{
  if (i == 0)
  {
    return mess.address.device;
  }
  else if (i < mess.address.path.size())
  {
    return mess.address.path[i - 1];
  }
  else
  {
    return mess.address.path.back();
  }
}

// TODO another one to refactor with merges
// MergeFun takes a state node value and modifies it.
template <typename MergeFun>
static void merge_impl(
    Process::MessageNode& base, const State::AddressAccessor& addr,
    MergeFun merge)
{
  const auto path_n = addr.address.path.size() + 1;

  Process::MessageNode* node = &base;
  for (int i = 0; i < path_n; i++)
  {
    auto it = ossia::find_if(*node, [&](const auto& cur_node) {
      return match(cur_node.name, addr, i);
    });

    if (it == node->end())
    {
      // We have to start adding sub-nodes from here.
      Process::MessageNode* parentnode{node};
      for (int k = i; k < path_n; k++)
      {
        Process::MessageNode* newNode{};
        if (k < path_n - 1)
        {
          newNode = &parentnode->emplace_back(
              Process::StateNodeData{get_at(addr, k), {}}, nullptr);
        }
        else
        {
          Process::StateNodeValues v;
          merge(v);
          newNode = &parentnode->emplace_back(
              Process::StateNodeData{get_at(addr, k), std::move(v)}, nullptr);
        }

        parentnode = newNode;
      }

      break;
    }
    else
    {
      node = &*it;

      if (i == path_n - 1)
      {
        // We replace the value by the one in the message
        merge(node->values);
      }
    }
  }
}

void updateTreeWithMessageList(
    Process::MessageNode& rootNode, State::MessageList lst)
{
  for (const auto& mess : lst)
  {
    merge_impl(rootNode, mess.address, [&](Process::StateNodeValues& nodeValues)
    {
      // TODO unit conversions
      const auto& acc = mess.address.qualifiers.get().accessors;
      if(acc.empty())
      {
        nodeValues.userValue.clear();
        nodeValues.userValue.push_back({mess.address.qualifiers, mess.value});
      }
      else
      {
        bool ok = false;
        for(auto& v : nodeValues.userValue)
        {
          auto& existing_acc = v.qualifiers.get().accessors;
          if(!existing_acc.empty() && existing_acc[0] == acc[0])
          {
            v.value = mess.value;
            ok = true;
            break;
          }
        }
        if(!ok)
          nodeValues.userValue.push_back({mess.address.qualifiers, mess.value});
      }
    });
  }
}

void updateTreeWithRemovedUserMessage(
    Process::MessageNode& rootNode, const State::AddressAccessor& addr)
{
  // Find the message node
  Process::MessageNode* node = try_getNodeFromString(rootNode, addr);

  if (node)
  {
    node->values.userValue.clear(); // TODO remove the relevant part

    // If it is empty, delete it.
    if (node->childCount() == 0)
    {
      rec_delete(*node);
    }
  }
}

static void rec_removeUserValue(Process::MessageNode& node)
{
  // Recursively set the user value to nil.
  node.values.userValue.clear(); // TODO remove the relevant part

  for (auto& child : node)
  {
    rec_removeUserValue(child);
  }
}

static bool rec_cleanup(Process::MessageNode& node)
{
  std::set<const Process::MessageNode*> toRemove;
  for (auto& child : node)
  {
    bool canEraseChild = rec_cleanup(child);
    if (canEraseChild)
    {
      toRemove.insert(&child);
    }
  }

  auto remove_it = ossia::remove_if(node, [&](const auto& child) {
    return toRemove.find(&child) != toRemove.end();
  });
  node.erase(remove_it, node.end());

  return node.childCount() == 0;
}




void updateTreeWithRemovedNode(
    Process::MessageNode& rootNode, const State::AddressAccessor& addr)
{
  // Find the message node
  Process::MessageNode* node_ptr = try_getNodeFromString(rootNode, addr);

  if (node_ptr)
  {
    auto& node = *node_ptr;
    // Recursively set the user value to nil.
    rec_removeUserValue(node);

    // If it is empty, delete it
    rec_cleanup(node);

    if (!node.hasChildren())
    {
      rec_delete(node);
    }
  }
}

/// Functions related to removal of user messages ///
int countNodes(Process::MessageNode& rootNode)
{
  int n = 0;
  for (auto& child : rootNode)
  {
    if (child.hasValue())
      n++;
    n += 1 + countNodes(child);
  }
  return n;
}

static Process::MessageNode*
rec_getNthChild(Process::MessageNode& rootNode, int& n)
{
  for (auto& child : rootNode)
  {
    if (child.hasValue())
      n--;
    if (n == 0)
      return &child;

    if (auto ptr = rec_getNthChild(child, n))
      return ptr;
  }
  return nullptr;
}

Process::MessageNode* getNthChild(Process::MessageNode& rootNode, int n)
{
  return rec_getNthChild(rootNode, n);
}

static void rec_getChildIndex(
    Process::MessageNode& rootNode, Process::MessageNode* n, int& idx)
{
  for (auto& child : rootNode)
  {
    if (child.hasValue())
      idx++;
    if (&child == n)
      return;
    rec_getChildIndex(child, n, idx);
  }
}

int getChildIndex(Process::MessageNode& rootNode, Process::MessageNode* node)
{
  int n = 0;
  rec_getChildIndex(rootNode, node, n);
  return n;
}
}
