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

// TODO reflist may be a better name.
using NodeList = QList<iscore::Node*>;

// TODO add specifications & tests to these functions
iscore::Address address(const Node& treeNode);

iscore::Message message(const iscore::Node& node);

// Note : this one takes an output reference as an optimization
// because of its use in DeviceExplorerModel::indexesToMime
void messageList(const Node& treeNode,
                 MessageList& ml);

// TODO have all these guys return references
iscore::Node& getNodeFromAddress(iscore::Node& root, const iscore::Address&);
iscore::Node* getNodeFromString(iscore::Node& n, QStringList&& str); // Fails if not present.

// True if gramps is a parent, grand-parent, etc. of node.
bool isAncestor(const iscore::Node& gramps, iscore::Node* node);


/**
 * @brief dumpTree An utility to print trees
 * of iscore::Nodes
 */
void dumpTree(const iscore::Node& node, QString rec);


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
QList<iscore::Node*> filterUniqueParents(const QList<iscore::Node*>& nodes);


iscore::Node merge(
        iscore::Node base,
        const iscore::MessageList& other);

void merge(
        iscore::Node& base,
        const iscore::Message& message);



// Generic algorithms for DeviceExplorerNode-like structures.
template<typename Node_T>
Node_T* try_getNodeFromString(Node_T& n, QStringList&& parts)
{
    if(parts.size() == 0)
        return &n;

    for(auto& child : n)
    {
        if(child.displayName() == parts[0])
        {
            parts.removeFirst();
            return try_getNodeFromString(child, std::move(parts));
        }
    }

    return nullptr;
}

template<typename Node_T>
Node_T* try_getNodeFromAddress(Node_T& root, const iscore::Address& addr)
{
    if(addr.device.isEmpty())
        return &root;

    auto dev = std::find_if(root.begin(), root.end(), [&] (const Node_T& n)
    { return n.template is<DeviceSettings>()
          && n.template get<DeviceSettings>().name == addr.device; });

    if(dev == root.end())
        return nullptr;

    return try_getNodeFromString(*dev, QStringList(addr.path));
}

}

