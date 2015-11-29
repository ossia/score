#pragma once
#include <State/Message.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/TreePath.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <QList>
#include <QString>
#include <QStringList>
#include <algorithm>

#include <State/Address.hpp>

class DataStream;
class JSONObject;
namespace iscore {
struct AddressSettings;
struct DeviceSettings;
}  // namespace iscore

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



/**
 * @brief dumpTree An utility to print trees
 * of iscore::Nodes
 */
void dumpTree(const iscore::Node& node, QString rec);




iscore::Node merge(
        iscore::Node base,
        const iscore::MessageList& other);

void merge(
        iscore::Node& base,
        const iscore::Message& message);


// True if gramps is a parent, grand-parent, etc. of node.
template<typename Node_T>
bool isAncestor(const Node_T& gramps, const Node_T* node)
{
    auto parent = node->parent();
    if(!parent)
        return false;

    if(node == &gramps)
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
template<typename Node_T>
QList<Node_T*> filterUniqueParents(const QList<Node_T*>& nodes)
{
    // OPTIMIZEME this horrible lazy algorithm.
    auto nodes_cpy = nodes.toSet().toList(); // Remove duplicates

    QList<Node_T*> cleaned_nodes;

    // Only copy the index if it none of its parents
    // except the invisible root are in the list.
    for(auto n : nodes_cpy)
    {
        if(std::any_of(nodes_cpy.begin(), nodes_cpy.end(),
                       [&] (Node_T* other) {
              if(other == n)
                  return false;
              return isAncestor(*other, n);
           }))
        {
            nodes_cpy.removeOne(n);
        }
        else
        {
            cleaned_nodes.append(n);
        }
    }

    return cleaned_nodes;
}

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

