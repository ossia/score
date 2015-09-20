#pragma once
#include <QString>
#include <QList>
#include <QJsonObject>

#include <State/Message.hpp>

#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/VariantBasedNode.hpp>

#include <iscore/tools/TreePath.hpp>
namespace iscore
{
class DeviceExplorerNode : public VariantBasedNode<
        iscore::DeviceSettings,
        iscore::AddressSettings>
{
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, DataStream)
        ISCORE_SERIALIZE_FRIENDS(DeviceExplorerNode, JSONObject)

    public:
            enum class Type { RootNode, Device, Address };

        DeviceExplorerNode(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode(DeviceExplorerNode&& t) = default;
        DeviceExplorerNode& operator=(const DeviceExplorerNode& t) = default;
        DeviceExplorerNode() = default;
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
using NodePath = TreePath<iscore::Node>;

// TODO add specifications & tests to these functions
iscore::Address address(const Node& treeNode);

iscore::Message message(const iscore::Node& node);

// Note : this one takes an output reference as an optimization
// because of its use in DeviceExplorerModel::indexesToMime
void messageList(const Node& treeNode,
                 MessageList& ml);

iscore::Node* try_getNodeFromAddress(iscore::Node& root, const iscore::Address&);
iscore::Node* try_getNodeFromString(iscore::Node& n, QStringList&& str);
iscore::Node* getNodeFromString(iscore::Node& n, QStringList&& str); // Fails if not present.

// True if gramps is a parent, grand-parent, etc. of node.
bool isAncestor(const iscore::Node& gramps, iscore::Node* node);

}

