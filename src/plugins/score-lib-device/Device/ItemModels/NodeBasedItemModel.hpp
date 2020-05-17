#pragma once
#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreeNodeItemModel.hpp>

#include <score_lib_device_export.h>

#include <vector>

namespace Device
{
class SCORE_LIB_DEVICE_EXPORT NodeBasedItemModel : public TreeNodeBasedItemModel<Device::Node>
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
      SCORE_ASSERT(n.parent());
      return createIndex(n.parent()->indexOfChild(&n), 0, &n);
    }
    else
    {
      node_type* parent = n.parent();
      SCORE_ASSERT(parent);
      SCORE_ASSERT(parent != &rootNode());

      return createIndex(parent->indexOfChild(&n), column, &n);
    }
  }

  void insertNode(node_type& parentNode, const node_type& other, int row)
  {
    QModelIndex parentIndex = modelIndexFromNode(parentNode, 0);

    beginInsertRows(parentIndex, row, row);

    auto it = parentNode.begin();
    std::advance(it, row);
    parentNode.emplace(it, other, &parentNode);

    endInsertRows();
  }

  auto removeNode(node_type::const_iterator node)
  {
    SCORE_ASSERT(!node->is<InvisibleRootNode>());

    if (node->is<Device::AddressSettings>())
    {
      node_type* parent = node->parent();
      SCORE_ASSERT(parent);
      SCORE_ASSERT(parent != &rootNode());
      node_type* grandparent = parent->parent();
      SCORE_ASSERT(grandparent);
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
      SCORE_ASSERT(node->is<Device::DeviceSettings>());
      int row = rootNode().indexOfChild(&*node);

      beginRemoveRows(QModelIndex(), row, row);
      auto it = rootNode().erase(node);
      endRemoveRows();
      return it;
    }
  }
};

SCORE_LIB_DEVICE_EXPORT Device::FullAddressAccessorSettings makeFullAddressAccessorSettings(
    const State::AddressAccessor& mess,
    const Device::NodeBasedItemModel& ctx,
    ossia::value min,
    ossia::value max);
SCORE_LIB_DEVICE_EXPORT Device::FullAddressAccessorSettings
makeFullAddressAccessorSettings(const Device::Node& mess);
}
