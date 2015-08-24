#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/VariantBasedNode.hpp>

namespace iscore
{

// TODO rename the file
class DeviceExplorerNode : public VariantBasedNode<
        iscore::DeviceSettings,
        iscore::AddressSettings,
        InvisibleRootNodeTag>
{
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, JSONObject)

    public:
            enum class Type { Device, Address, RootNode };

        DeviceExplorerNode(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode(DeviceExplorerNode&& t) = default;
        DeviceExplorerNode& operator=(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode():
            VariantBasedNode{InvisibleRootNodeTag{}}
        {

        }

        template<typename T>
        DeviceExplorerNode(const T& t):
            VariantBasedNode{t}
        {

        }

        //- accessors
        QString displayName() const;

        bool isSelectable() const;
        bool isEditable() const;
};

using Node = TreeNode<DeviceExplorerNode>;

iscore::Address address(const Node& treeNode);

Node* try_getNodeFromString(Node* n, QStringList&& str);
Node* getNodeFromString(Node* n, QStringList&& str); // Fails if not present.
}
