#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <iscore/tools/TreeNodeItemModel.hpp>
#include <QAbstractItemModel>
#include <vector>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <iscore/tools/InvisibleRootNode.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore_lib_device_export.h>

class ISCORE_LIB_DEVICE_EXPORT NodeBasedItemModel : public TreeNodeBasedItemModel<iscore::Node>
{
    public:
        using TreeNodeBasedItemModel<iscore::Node>::TreeNodeBasedItemModel;
        virtual ~NodeBasedItemModel();

        QModelIndex modelIndexFromNode(node_type& n, int column) const
        {
            if(n.is<InvisibleRootNodeTag>())
            {
                return QModelIndex();
            }
            else if(n.is<iscore::DeviceSettings>())
            {
                ISCORE_ASSERT(n.parent());
                return index(n.parent()->indexOfChild(&n), 0, QModelIndex());
            }
            else
            {
                node_type* parent = n.parent();
                ISCORE_ASSERT(parent);
                ISCORE_ASSERT(parent != &rootNode());

                return createIndex(parent->indexOfChild(&n), column, &n);
            }
        }

        void insertNode(node_type& parentNode,
                        const node_type& other,
                        int row)
        {
            QModelIndex parentIndex = modelIndexFromNode(parentNode, 0);

            beginInsertRows(parentIndex, row, row);

            parentNode.emplace(parentNode.begin() + row, other, &parentNode);

            endInsertRows();
        }

        void removeNode(node_type::const_iterator node)
        {
            ISCORE_ASSERT(!node->is<InvisibleRootNodeTag>());

            if(node->is<iscore::AddressSettings>())
            {
                node_type* parent = node->parent();
                ISCORE_ASSERT(parent != &rootNode());
                node_type* grandparent = parent->parent();
                ISCORE_ASSERT(grandparent);
                int rowParent = grandparent->indexOfChild(parent);
                QModelIndex parentIndex = createIndex(rowParent, 0, parent);

                int row = parent->indexOfChild(&*node);

                beginRemoveRows(parentIndex, row, row);
                parent->removeChild(node);
                endRemoveRows();
            }
            else if(node->is<iscore::DeviceSettings>())
            {
                int row = rootNode().indexOfChild(&*node);

                beginRemoveRows(QModelIndex(), row, row);
                rootNode().removeChild(node);
                endRemoveRows();
            }
        }
};
