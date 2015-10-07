#include "DeviceExplorerNode.hpp"
#include <boost/range/algorithm/find_if.hpp>

namespace iscore
{
QString DeviceExplorerNode::displayName() const
{
    switch(m_data.which())
    {
        case (int)Type::Device:
            return get<DeviceSettings>().name;
            break;
        case (int)Type::Address:
            return get<AddressSettings>().name;
            break;
        case (int)Type::RootNode:
            return "Invisible Root DeviceExplorerNode";
            break;
        default:
            return "==ERROR==";
            break;
    }
}

bool DeviceExplorerNode::isSelectable() const
{
    return true;
}

bool DeviceExplorerNode::isEditable() const
{
    return is<AddressSettings>() && hasOutput(get<AddressSettings>().ioType);
}

iscore::Node* getNodeFromString(
        iscore::Node& n,
        QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    ISCORE_ASSERT(theN);
    return theN;
}

iscore::Node& getNodeFromAddress(
        iscore::Node& n,
        const iscore::Address& addr)
{
    auto theN = try_getNodeFromAddress(n, addr);
    ISCORE_ASSERT(theN);
    return *theN;
}

Address address(const Node &treeNode)
{
    Address addr;
    const Node* n = &treeNode;
    while(n->parent() && !n->is<DeviceSettings>())
    {
        addr.path.prepend(n->get<AddressSettings>().name);
        n = n->parent();
    }

    ISCORE_ASSERT(n);
    ISCORE_ASSERT(n->is<DeviceSettings>());
    addr.device = n->get<DeviceSettings>().name;

    return addr;
}

bool isAncestor(const Node& gramps, Node* node)
{
    // TODO why ??
    if(node->is<InvisibleRootNodeTag>())
        return false;

    if(node == &gramps)
        return true;

    return isAncestor(gramps, node->parent());
}

void messageList(const Node& n,
                 MessageList& ml)
{
    if(n.is<AddressSettings>())
    {
        const auto& stgs = n.get<AddressSettings>();

        // Note : this is an arbitrary choice. Discuss with
        // the users and see repercussions in MessageItemModel.
        if(hasOutput(stgs.ioType))
        {
            ml.push_back(message(n));
        }
    }

    for(const auto& child : n.children())
    {
        messageList(child, ml);
    }

}

Message message(const Node& node)
{
    if(!node.is<iscore::AddressSettings>())
        return {};

    iscore::Message mess;
    mess.address = address(node);
    mess.value = node.get<iscore::AddressSettings>().value;

    return mess;
}

QList<Node*> filterUniqueParents(const QList<Node*>& nodes)
{
    // OPTIMIZEME this horrible lazy algorithm.
    auto nodes_cpy = nodes.toSet().toList(); // Remove duplicates

    QList<iscore::Node*> cleaned_nodes;

    // Only copy the index if it none of its parents
    // except the invisible root are in the list.
    for(auto n : nodes_cpy)
    {
        if(std::any_of(nodes_cpy.begin(), nodes_cpy.end(),
                       [&] (iscore::Node* other) {
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


// TESTME
// TODO : this is really a pattern
// (see iscore2OSSIA, iscore_plugin_coppa and friends), try to refactor it.
// This could be a try_insert algorithm.
void merge(
        iscore::Node& base,
        const iscore::Message& message)
{
    using iscore::Node;

    QStringList path = message.address.path;
    path.prepend(message.address.device);

    ptr<Node> node = &base;
    for(int i = 0; i < path.size(); i++)
    {
        auto& children = node->children();
        auto it = std::find_if(
                    children.begin(), children.end(),
                    [&] (const auto& cur_node) {
            return cur_node.displayName() == path[i];
        });

        if(it == children.end())
        {
            // We have to start adding sub-nodes from here.
            ptr<Node> parentnode{node};
            for(int k = i; k < path.size(); k++)
            {
                ptr<Node> newNode;
                if(k == 0)
                {
                    // We're adding a device
                    iscore::DeviceSettings dev;
                    dev.name = path[k];
                    newNode = &parentnode->emplace_back(std::move(dev), nullptr);
                }
                else
                {
                    // We're adding an address
                    iscore::AddressSettings addr;
                    addr.name = path[k];

                    if(k == path.size() - 1)
                    {
                        // End of the address
                        addr.value = message.value;

                        // Note : since we don't have this
                        // information in messagelist's,
                        // we assign a default Out value
                        // so that we only send the nodes that actually had messages
                        // via the OSSIA api.
                        addr.ioType = IOType::Out;
                    }

                    newNode = &parentnode->emplace_back(std::move(addr), nullptr);
                }

                // TODO do similar simplification on other similar algorithms
                // cf in ossia stuff
                parentnode = newNode;
            }

            break;
        }
        else
        {
            node = &*it;

            if(i == path.size() - 1)
            {
                // We replace the value by the one in the message
                if(node->is<iscore::AddressSettings>())
                {
                    node->get<iscore::AddressSettings>().value = message.value;
                }
            }
        }
    }
}

// TODO why doesn't this take a reference ?
iscore::Node merge(
        iscore::Node base,
        const iscore::MessageList& other)
{
    using namespace iscore;
    // For each node in other, if we can also find a similar node in
    // base, we replace its data
    // Else, we insert it.

    ISCORE_ASSERT(base.is<InvisibleRootNodeTag>());

    for(const auto& message : other)
    {
        merge(base, message);
    }

    return base;
}

void dumpTree(const Node& node, QString rec)
{
    qDebug() << qUtf8Printable(rec) << qUtf8Printable(node.displayName());
    rec += " ";
    for(const auto& child : node)
    {
        dumpTree(child, rec);
    }
}

}

