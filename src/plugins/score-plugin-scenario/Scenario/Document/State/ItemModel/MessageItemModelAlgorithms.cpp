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
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QString>
#include <QStringList>

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
static Process::MessageNode* try_getNodeFromString_impl(
    Process::MessageNode& n,
    QStringList::const_iterator begin,
    QStringList::const_iterator end,
    const State::DestinationQualifiers& qual)
{
  if (begin == end)
  {
    return &n;
  }
  else
  {
    auto next = begin + 1;
    if (next == end)
    {
      for (auto& child : n)
      {
        if (child.name.name == *begin && child.name.qualifiers == qual)
        {
          return &child;
        }
      }
    }
    else
    {
      for (auto& child : n)
      {
        if (child.name.name == *begin)
        {
          return try_getNodeFromString_impl(child, next, end, qual);
        }
      }
    }
  }
  return nullptr;
}

static Process::MessageNode*
try_getNodeFromString(Process::MessageNode& n, const State::AddressAccessor& addr)
{
  for (auto& child : n)
  {
    if (child.displayName() == addr.address.device)
    {
      return try_getNodeFromString_impl(
          child, addr.address.path.begin(), addr.address.path.end(), addr.qualifiers);
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
    if (n->name.name == path.at(i))
    {
      if (i == imax - 1 && !n->parent()->parent() && mess.address.qualifiers == n->name.qualifiers)
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

static void updateNode(
    std::vector<Process::ProcessStateData>& vec,
    const ossia::value& val,
    const Id<Process::ProcessModel>& proc)
{
  auto it = ossia::find_if(vec, [&](auto& data) { return data.process == proc; });
  if (it != vec.end())
  {
    it->value = val;
  }
  else
  {
    vec.push_back({proc, val});
  }
}

static void rec_delete(Process::MessageNode& node)
{
  if (node.childCount() == 0)
  {
    // kthxbai
    auto parent = node.parent();
    if (parent)
    {
      auto it = std::find_if(
          parent->begin(), parent->end(), [&](const auto& other) { return &node == &other; });
      if (it != parent->end())
      {
        parent->erase(it);
        rec_delete(*parent);
      }
    }
  }
}

// Returns true if this node is to be deleted.
static bool nodePruneAction_impl(
    Process::MessageNode& node,
    const Id<Process::ProcessModel>& proc,
    std::vector<Process::ProcessStateData>& vec,
    const std::vector<Process::ProcessStateData>& other_vec)
{
  int vec_size = vec.size();
  if (vec_size > 1)
  {
    // We just remove the element
    // corresponding to this process.
    auto it = ossia::find_if(vec, [&](const auto& data) { return data.process == proc; });

    if (it != vec.end())
    {
      vec.erase(it);
    }
  }
  else if (vec_size == 1)
  {
    // We may be able to remove the whole node
    if (vec.front().process == proc)
      vec.clear();

    // If false, nothing is removed.
    return vec.empty() && other_vec.empty() && !node.values.userValue;
  }

  return false;
}

static void nodePruneAction(
    Process::MessageNode& node,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  // If there is no corresponding message in our list,
  // but there is a corresponding process in the tree,
  // we prune it
  bool deleteMe = node.childCount() == 0;
  switch (pos)
  {
    case ProcessPosition::Previous:
    {
      deleteMe &= nodePruneAction_impl(
          node, proc, node.values.previousProcessValues, node.values.followingProcessValues);
      break;
    }
    case ProcessPosition::Following:
    {
      deleteMe &= nodePruneAction_impl(
          node, proc, node.values.followingProcessValues, node.values.previousProcessValues);
      break;
    }
    default:
      SCORE_ABORT;
      break;
  }

  if (deleteMe)
  {
    node.values = Process::StateNodeValues{};
  }
}

static void nodeInsertAction(
    Process::MessageNode& node,
    State::MessageList& msg,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  auto it = msg.begin();
  auto end = msg.end();
  while (it != end)
  {
    const auto& mess = *it;
    if (match(node, mess))
    {
      switch (pos)
      {
        case ProcessPosition::Previous:
        {
          updateNode(node.values.previousProcessValues, mess.value, proc);
          break;
        }
        case ProcessPosition::Following:
        {
          updateNode(node.values.followingProcessValues, mess.value, proc);
          break;
        }
        default:
          SCORE_ABORT;
          break;
      }

      it = msg.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

static void rec_updateTree(
    Process::MessageNode& node,
    State::MessageList& lst,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  // If the message is in the tree, we add the process value.
  int n = lst.size();
  nodeInsertAction(node, lst, proc, pos);
  if (lst.size() == n) // No nodes were added / updated
  {
    nodePruneAction(node, proc, pos);
  }

  for (auto& child : node)
  {
    rec_updateTree(child, lst, proc, pos);
  }

  cleanupNode(node);
}

static bool
match(const State::AddressAccessorHead& cur_node, const State::AddressAccessor& mess, int i)
{
  if (i == 0)
  {
    return mess.address.device == cur_node.name;
  }
  else if (i < mess.address.path.size())
  {
    return mess.address.path[i - 1] == cur_node.name;
  }
  else
  {
    return mess.address.path.back() == cur_node.name && mess.qualifiers == cur_node.qualifiers;
  }
}

static State::AddressAccessorHead get_at(const State::AddressAccessor& mess, int i)
{
  if (i == 0)
  {
    return {mess.address.device, {}};
  }
  else if (i < mess.address.path.size())
  {
    return {mess.address.path[i - 1], {}};
  }
  else
  {
    return {mess.address.path.back(), mess.qualifiers};
  }
}

// TODO another one to refactor with merges
// MergeFun takes a state node value and modifies it.
template <typename MergeFun>
static void
merge_impl(Process::MessageNode& base, const State::AddressAccessor& addr, MergeFun merge)
{
  const auto path_n = addr.address.path.size() + 1;

  Process::MessageNode* node = &base;
  for (int i = 0; i < path_n; i++)
  {
    auto it = ossia::find_if(
        *node, [&](const auto& cur_node) { return match(cur_node.name, addr, i); });

    if (it == node->end())
    {
      // We have to start adding sub-nodes from here.
      Process::MessageNode* parentnode{node};
      for (int k = i; k < path_n; k++)
      {
        Process::MessageNode* newNode{};
        if (k < path_n - 1)
        {
          newNode
              = &parentnode->emplace_back(Process::StateNodeData{get_at(addr, k), {}}, nullptr);
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

static void
rename_impl(Process::MessageNode& base, const State::AddressAccessor& old_addr, const State::AddressAccessor& new_addr)
{
  const auto path_n = old_addr.address.path.size() + 1;

  Process::MessageNode* node = &base;
  for (int i = 0; i < path_n; i++)
  {
    auto it = ossia::find_if(
        *node, [&](const auto& cur_node) { return match(cur_node.name, old_addr, i); });

    if (it != node->end())
    {
      node = &*it;

      if (i == path_n - 1)
      {
        if(new_addr.address.path.empty())
          node->name.name = new_addr.address.device;
        else
          node->name.name = new_addr.address.path.back();

        node->name.qualifiers = new_addr.qualifiers;
      }
    }
  }
}

void updateTreeWithMessageList(Process::MessageNode& rootNode, State::MessageList lst)
{
  for (const auto& mess : lst)
  {
    merge_impl(
        rootNode, mess.address, [&](auto& nodeValues) { nodeValues.userValue = mess.value; });
  }
}

void updateTreeWithMessageList(
    Process::MessageNode& rootNode,
    State::MessageList lst,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  // We go through the tree.
  // For each node :
  // If lst contains a message corresponding to the node, we add / update it
  // (and remove it from lst).
  // If the node contains a message corresponding to the process but there is
  // none in lst, we remove its process part
  // (and we remove it if it's empty (and we also remove all the parents that
  // don't have a value recursively)

  // When the tree has been visited, if there are remaining addresses in lst,
  // we insert them in the tree.

  // We don't perform anything on the root node
  for (auto& child : rootNode)
  {
    rec_updateTree(child, lst, proc, pos);
  }

  // Handle the remaining messages
  for (const auto& mess : lst)
  {
    merge_impl(rootNode, mess.address, [&](Process::StateNodeValues& nodeValues) {
      switch (pos)
      {
        case ProcessPosition::Previous:
          nodeValues.previousProcessValues.push_back({proc, mess.value});
          break;
        case ProcessPosition::Following:
          nodeValues.followingProcessValues.push_back({proc, mess.value});
          break;
        default:
          SCORE_ABORT;
          break;
      }
    });
  }
}

static void rec_pruneTree(
    Process::MessageNode& node,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  // If the message is in the tree, we remove the process value.
  nodePruneAction(node, proc, pos);

  for (auto& child : node)
  {
    rec_pruneTree(child, proc, pos);
  }

  cleanupNode(node);
}

void updateTreeWithRemovedProcess(
    Process::MessageNode& rootNode,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  for (auto& child : rootNode)
  {
    rec_pruneTree(child, proc, pos);
  }

  cleanupNode(rootNode);
}

static void nodePruneAction(Process::MessageNode& node, ProcessPosition pos)
{
  // If there is no corresponding message in our list,
  // but there is a corresponding process in the tree,
  // we prune it
  bool deleteMe = node.childCount() == 0;
  switch (pos)
  {
    case ProcessPosition::Previous:
    {
      node.values.previousProcessValues.clear();
      deleteMe &= !node.values.userValue && node.values.followingProcessValues.empty();
      break;
    }
    case ProcessPosition::Following:
    {
      node.values.followingProcessValues.clear();
      deleteMe &= !node.values.userValue && node.values.previousProcessValues.empty();
      break;
    }
    default:
      SCORE_ABORT;
      break;
  }

  if (deleteMe)
  {
    node.values = Process::StateNodeValues{};
  }
}

static void rec_pruneTree(Process::MessageNode& node, ProcessPosition pos)
{
  nodePruneAction(node, pos);

  for (auto& child : node)
  {
    rec_pruneTree(child, pos);
  }

  cleanupNode(node);
}

void updateTreeWithRemovedInterval(Process::MessageNode& rootNode, ProcessPosition pos)
{
  for (auto& child : rootNode)
  {
    rec_pruneTree(child, pos);
  }
}

void updateTreeWithRemovedUserMessage(
    Process::MessageNode& rootNode,
    const State::AddressAccessor& addr)
{
  // Find the message node
  Process::MessageNode* node = try_getNodeFromString(rootNode, addr);

  if (node)
  {
    node->values.userValue = std::optional<ossia::value>{};

    // If it is empty, delete it.
    if (node->values.previousProcessValues.empty() && node->values.followingProcessValues.empty()
        && node->childCount() == 0)
    {
      rec_delete(*node);
    }
  }
}

static void rec_removeUserValue(Process::MessageNode& node)
{
  // Recursively set the user value to nil.
  node.values.userValue = std::optional<ossia::value>{};

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

  auto remove_it = ossia::remove_if(
      node, [&](const auto& child) { return toRemove.find(&child) != toRemove.end(); });
  node.erase(remove_it, node.end());

  return node.values.previousProcessValues.empty() && node.values.followingProcessValues.empty()
         && node.childCount() == 0;
}

void updateTreeWithRemovedNode(Process::MessageNode& rootNode, const State::AddressAccessor& addr)
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

    if (node.values.previousProcessValues.empty() && node.values.followingProcessValues.empty()
        && node.childCount() == 0)
    {
      rec_delete(node);
    }
  }
}

/// Functions related to removal of user messages ///
static void nodePruneAction(Process::MessageNode& node)
{
  node.values.userValue = std::optional<ossia::value>{};
}

static void rec_pruneTree(Process::MessageNode& node)
{
  // First set all the user messages to "empty"
  nodePruneAction(node);

  // Recurse
  for (auto& child : node)
  {
    rec_pruneTree(child);
  }

  // Then try removing everything that does not have a message.
  cleanupNode(node);
}

void removeAllUserMessages(Process::MessageNode& rootNode)
{
  for (auto& child : rootNode)
  {
    rec_pruneTree(child);
  }

  cleanupNode(rootNode);
}

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

static Process::MessageNode* rec_getNthChild(Process::MessageNode& rootNode, int& n)
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

static void rec_getChildIndex(Process::MessageNode& rootNode, Process::MessageNode* n, int& idx)
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

void renameAddress(Process::MessageNode& rootNode, const State::AddressAccessor& oldAddr, const State::AddressAccessor& newAddr)
{
  rename_impl(rootNode, oldAddr, newAddr);
}

}
