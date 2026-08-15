#pragma once
#include <QAbstractItemModel>

#include <score_lib_base_export.h>

/**
 * @brief Base implementation of a tree for QAbstractItemModel
 *
 * Provides basic tree-like functionality
 * shared between item models that uses the NodeType.
 */
// TESTME
class TreePath;
class SCORE_LIB_BASE_EXPORT TreeModel : public QAbstractItemModel
{
public:
  using QAbstractItemModel::QAbstractItemModel;
  //! idx: should be the root index of the view
  template <typename F>
  void iterate(const QModelIndex& idx, const F& f)
  {
    if(idx.isValid())
      f(idx);

    if(!hasChildren(idx))
      return;

    const int rows = rowCount(idx);
    for(int i = 0; i < rows; ++i)
      iterate(this->index(i, 0, idx), f);
  }

  QModelIndex convertPathToIndex(const TreePath& path) const;
};

template <typename NodeType>
class TreeNodeBasedItemModel : public TreeModel
{
public:
  explicit TreeNodeBasedItemModel(QObject* parent = nullptr)
      : TreeModel{parent}
  {
    // The row caches map rows to child nodes; any structural change may
    // shift rows or erase cached elements. These connections are made
    // before any observer can connect, so the caches die before an
    // observer gets to query the post-mutation model.
    auto invalidate = [this] {
      m_rowCache[0].parent = nullptr;
      m_rowCache[1].parent = nullptr;
    };
    connect(this, &QAbstractItemModel::rowsInserted, this, invalidate);
    connect(this, &QAbstractItemModel::rowsRemoved, this, invalidate);
    connect(this, &QAbstractItemModel::rowsMoved, this, invalidate);
    connect(this, &QAbstractItemModel::modelReset, this, invalidate);
    connect(this, &QAbstractItemModel::layoutChanged, this, invalidate);
  }

  using node_type = NodeType;
  virtual ~TreeNodeBasedItemModel() = default;
  virtual NodeType& rootNode() = 0;
  virtual const NodeType& rootNode() const = 0;

  NodeType& nodeFromModelIndex(const QModelIndex& index) const
  {
    auto n = index.isValid() ? static_cast<NodeType*>(index.internalPointer())
                             : const_cast<NodeType*>(&rootNode());

    SCORE_ASSERT(n);
    return *n;
  }

  QModelIndex parent(const QModelIndex& index) const final override
  {
    if(!index.isValid())
      return QModelIndex();
    if(index.model() != this)
      return QModelIndex();

    const auto& node = nodeFromModelIndex(index);
    auto parentNode = node.parent();

    if(!parentNode)
      return QModelIndex();

    auto grandparentNode = parentNode->parent();

    if(!grandparentNode)
      return QModelIndex();

    const int rowParent = grandparentNode->indexOfChild(parentNode);
    if(rowParent == -1)
      return QModelIndex();

    return createIndex(rowParent, 0, parentNode);
  }

  QModelIndex index(int row, int column, const QModelIndex& parent) const final override
  {
    if(!hasIndex(row, column, parent))
      return QModelIndex();

    auto& parentItem = nodeFromModelIndex(parent);
    if(!parentItem.hasChild(row))
      return QModelIndex();

    // Children live in a std::list: childAt(row) is O(row), which makes any
    // consumer that resolves many rows of one parent - a view painting, a
    // QSortFilterProxyModel building a mapping, persistent-index updates
    // after an insert - O(n²) overall. Cache the row → node table for the
    // two most recently used parents (two, so a parent/child walk does not
    // thrash); structural changes invalidate through the signal connections
    // made in the constructor.
    auto* cache = &m_rowCache[0];
    if(m_rowCache[0].parent != &parentItem)
    {
      if(m_rowCache[1].parent == &parentItem)
      {
        cache = &m_rowCache[1];
      }
      else
      {
        // Replace the least-recently-used slot
        cache = (m_lastUsed == 0) ? &m_rowCache[1] : &m_rowCache[0];
        cache->parent = &parentItem;
        cache->rows.clear();
        cache->rows.reserve(parentItem.childCount());
        for(auto& child : parentItem)
          cache->rows.push_back(&child);
      }
    }
    m_lastUsed = int(cache - &m_rowCache[0]);
    return createIndex(row, column, cache->rows[row]);
  }

  int rowCount(const QModelIndex& parent) const final override
  {
    if(parent.column() > 0)
      return 0;

    const auto& parentNode = nodeFromModelIndex(parent);
    return parentNode.childCount();
  }

  bool hasChildren(const QModelIndex& parent) const final override
  {
    const auto& parentNode = nodeFromModelIndex(parent);
    return parentNode.childCount() > 0;
  }

private:
  struct RowCache
  {
    NodeType* parent{};
    std::vector<NodeType*> rows;
  };
  mutable RowCache m_rowCache[2];
  mutable int m_lastUsed{};
};
