#pragma once
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>
#include <QTreeView>
#include <QAbstractProxyModel>
#include <vector>

namespace score
{
template<typename Node, typename NodePath>
struct TreeViewExpandState
{
  void save(QAbstractProxyModel* m, QTreeView* v)
  {
    auto de = static_cast<TreeModel*>(m->sourceModel());

    m_expandedIndices.clear();
    de->iterate(v->rootIndex(), [this, m, v] (const QModelIndex& index) {
      if (v->isExpanded(m->mapFromSource(index)))
      {
        if(auto item = static_cast<Node*>(index.internalPointer()))
        {
          m_expandedIndices.push_back(NodePath{*item});
        }
      }
    });
  }

  void restore(QAbstractProxyModel* m, QTreeView* v)
  {
    auto de = static_cast<TreeModel*>(m->sourceModel());

    v->setUpdatesEnabled(false);
    v->collapseAll();
    for(auto& path : m_expandedIndices)
    {
      auto idx = de->convertPathToIndex(path);
      if(idx.isValid())
      {
        v->expand(m->mapFromSource(idx));
      }
    }
    v->setUpdatesEnabled(true);
    v->update();
  }

  std::vector<NodePath> m_expandedIndices;
};
}
