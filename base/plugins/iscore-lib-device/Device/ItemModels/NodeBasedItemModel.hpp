#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QAbstractItemModel>
#include <iscore/model/tree/TreeNodeItemModel.hpp>
#include <vector>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <iscore/model/tree/InvisibleRootNode.hpp>
#include <iscore/model/tree/TreeNode.hpp>
#include <iscore_lib_device_export.h>

namespace Device
{
class ISCORE_LIB_DEVICE_EXPORT NodeBasedItemModel
    : public TreeNodeBasedItemModel<Device::Node>
{
public:
  using TreeNodeBasedItemModel<Device::Node>::TreeNodeBasedItemModel;
  virtual ~NodeBasedItemModel();

  QModelIndex modelIndexFromNode(node_type& n, int column) const
  {
    if (n.is<InvisibleRootNode>())
    {
      return QModelIndex();
    }
    else if (n.is<Device::DeviceSettings>())
    {
      ISCORE_ASSERT(n.parent());
      return createIndex(n.parent()->indexOfChild(&n), 0, &n);
    }
    else
    {
      node_type* parent = n.parent();
      ISCORE_ASSERT(parent);
      ISCORE_ASSERT(parent != &rootNode());

      return createIndex(parent->indexOfChild(&n), column, &n);
    }
  }

  void insertNode(node_type& parentNode, const node_type& other, int row)
  {
    QModelIndex parentIndex = modelIndexFromNode(parentNode, 0);

    beginInsertRows(parentIndex, row, row);

    parentNode.emplace(parentNode.begin() + row, other, &parentNode);

    endInsertRows();
  }

  auto removeNode(node_type::const_iterator node)
  {
    ISCORE_ASSERT(!node->is<InvisibleRootNode>());

    if (node->is<Device::AddressSettings>())
    {
      node_type* parent = node->parent();
      ISCORE_ASSERT(parent);
      ISCORE_ASSERT(parent != &rootNode());
      node_type* grandparent = parent->parent();
      ISCORE_ASSERT(grandparent);
      int rowParent = grandparent->indexOfChild(parent);
      QModelIndex parentIndex = createIndex(rowParent, 0, parent);

      int row = parent->indexOfChild(&*node);

      beginRemoveRows(parentIndex, row, row);
      auto it = parent->erase(node);
      endRemoveRows();
      return it;
    }
    else
    {
      ISCORE_ASSERT(node->is<Device::DeviceSettings>());
      int row = rootNode().indexOfChild(&*node);

      beginRemoveRows(QModelIndex(), row, row);
      auto it = rootNode().erase(node);
      endRemoveRows();
      return it;
    }
  }
};
}
