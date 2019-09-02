#pragma once
#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

#include <list>
#include <vector>

/**
 * @brief Base type for a tree data structure.
 *
 * This class adds a tree structure around a data type.
 * It can then be used in abstract item models easily.
 */

template <typename T>
auto& child_at(const std::list<T>& list, int index)
{
  SCORE_ASSERT(index >= 0 && index < (int)list.size());
  auto it = list.begin();
  std::advance(it, index);
  return *it;
}

template <typename T>
auto& child_at(std::list<T>& list, int index)
{
  SCORE_ASSERT(index >= 0 && index < (int)list.size());
  auto it = list.begin();
  std::advance(it, index);
  return *it;
}

template <typename T>
int index_of_child(const std::list<T>& list, const T* child)
{
  int i = 0;
  const auto end = list.end();
  for (auto it = list.begin(); it != end; ++it)
  {
    if (&(*it) == child)
      return i;
    i++;
  }

  return -1;
}

template <typename DataType>
class TreeNode : public DataType
{
private:
  TreeNode* m_parent{};
  std::list<TreeNode> m_children;
  using impl_type = std::list<TreeNode>;

public:
  using iterator = typename impl_type::iterator;
  using const_iterator = typename impl_type::const_iterator;

  auto begin() { return m_children.begin(); }
  auto begin() const { return cbegin(); }
  auto cbegin() const { return m_children.cbegin(); }

  auto end() { return m_children.end(); }
  auto end() const { return cend(); }
  auto cend() const { return m_children.cend(); }

  TreeNode() = default;

  // The parent has to be set afterwards.
  TreeNode(const TreeNode& other)
      : DataType(static_cast<const DataType&>(other))
      , m_parent{other.m_parent}
      , m_children(other.m_children)
  {
    for (auto& child : m_children)
      child.setParent(this);
  }

  TreeNode(TreeNode&& other)
      : DataType(std::move(static_cast<DataType&&>(other)))
      , m_parent{other.m_parent}
      , m_children(std::move(other.m_children))
  {
    for (auto& child : m_children)
      child.setParent(this);
  }

  TreeNode& operator=(const TreeNode& source)
  {
    static_cast<DataType&>(*this) = static_cast<const DataType&>(source);
    m_parent = source.m_parent;

    m_children = source.m_children;
    for (auto& child : m_children)
    {
      child.setParent(this);
    }

    return *this;
  }

  TreeNode& operator=(TreeNode&& source)
  {
    static_cast<DataType&>(*this) = static_cast<DataType&&>(source);
    m_parent = source.m_parent;

    m_children = std::move(source.m_children);
    for (auto& child : m_children)
    {
      child.setParent(this);
    }

    return *this;
  }

  TreeNode(DataType data, TreeNode* parent)
      : DataType(std::move(data)), m_parent{parent}
  {
  }

  // Clone
  explicit TreeNode(TreeNode source, TreeNode* parent)
      : TreeNode{std::move(source)}
  {
    m_parent = parent;
  }

  void push_back(const TreeNode& child)
  {
    m_children.push_back(child);

    auto& cld = m_children.back();
    cld.setParent(this);
  }

  void push_back(TreeNode&& child)
  {
    m_children.push_back(std::move(child));

    auto& cld = m_children.back();
    cld.setParent(this);
  }

  template <typename... Args>
  auto& emplace_back(Args&&... args)
  {
    m_children.emplace_back(std::forward<Args>(args)...);

    auto& cld = m_children.back();
    cld.setParent(this);
    return cld;
  }

  template <typename... Args>
  auto& emplace(Args&&... args)
  {
    auto& n = *m_children.emplace(std::forward<Args>(args)...);
    n.setParent(this);
    return n;
  }

  TreeNode* parent() const { return m_parent; }

  bool hasChild(std::size_t index) const { return m_children.size() > index; }

  TreeNode& childAt(int index) { return child_at(m_children, index); }

  const TreeNode& childAt(int index) const
  {
    return child_at(m_children, index);
  }

  // returns -1 if not found
  int indexOfChild(const TreeNode* child) const
  {
    return index_of_child(m_children, child);
  }

  auto iterOfChild(const TreeNode* child)
  {
    const auto end = m_children.end();
    for (auto it = m_children.begin(); it != end; ++it)
    {
      if (&*it == child)
        return it;
    }
    return end;
  }

  int childCount() const { return m_children.size(); }

  bool hasChildren() const { return !m_children.empty(); }

  const auto& children() const { return m_children; }
  void reserve(std::size_t s)
  {
    // m_children.reserve(s);
  }
  void resize(std::size_t s) { m_children.resize(s); }

  auto erase(const_iterator it) { return m_children.erase(it); }

  auto erase(const_iterator it_beg, const_iterator it_end)
  {
    return m_children.erase(it_beg, it_end);
  }

  void setParent(TreeNode* parent) { m_parent = parent; }

  template <typename Fun>
  void visit(Fun f) const
  {
    f(*this);

    for (const auto& child : m_children)
    {
      child.visit(f);
    }
  }
};

// True if gramps is a parent, grand-parent, etc. of node.
template <typename Node_T>
bool isAncestor(const Node_T& gramps, const Node_T* node)
{
  auto parent = node->parent();
  if (!parent)
    return false;

  if (node == &gramps)
    return true;

  return isAncestor(gramps, parent);
}

/**
 * @brief filterUniqueParents
 * @param nodes A list of nodes
 * @return Another list of nodes
 *
 * This function filters a list of node
 * by only keeping the nodes that had no ancestor.
 *
 * e.g. given the tree :
 *
 * a -> b -> d
 *        -> e
 *   -> c
 * f -> g
 *
 * If the input consists of b, d, the output will be b.
 * If the input consists of a, b, d, f, the output will be a, f.
 * If the input consists of d, e, the output will be d, e.
 *
 * TESTME
 */
template <typename Node_T>
std::vector<Node_T*> filterUniqueParents(std::vector<Node_T*>& nodes)
{
  std::vector<Node_T*> cleaned_nodes;

  ossia::sort(nodes);
  nodes.erase(ossia::unique(nodes), nodes.end());

  cleaned_nodes.reserve(nodes.size());

  // Only copy the index if it none of its parents
  // except the invisible root are in the list.
  for (auto n : nodes)
  {
    if (ossia::any_of(nodes, [&](Node_T* other) {
          if (other == n)
            return false;
          return isAncestor(*other, n);
        }))
    {
      continue;
    }
    else
    {
      cleaned_nodes.push_back(n);
    }
  }

  return cleaned_nodes;
}
