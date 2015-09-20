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
    return is<AddressSettings>()
            && (get<AddressSettings>().ioType == IOType::InOut
               ||  get<AddressSettings>().ioType == IOType::Out);
}

iscore::Node* getNodeFromString(iscore::Node& n, QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    ISCORE_ASSERT(theN);
    return theN;
}

Node* try_getNodeFromAddress(iscore::Node& root, const Address& addr)
{
    if(addr.device.isEmpty() || addr.path.isEmpty())
        return nullptr;

    using namespace boost::range;
    auto dev = std::find_if(root.begin(), root.end(), [&] (const iscore::Node& n)
    { return n.is<DeviceSettings>() && n.get<DeviceSettings>().name == addr.device; });

    if(dev == root.end())
        return nullptr;

    return try_getNodeFromString(*dev, QStringList(addr.path));
}


iscore::Node* try_getNodeFromString(iscore::Node& n, QStringList&& parts)
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
        if(stgs.ioType == IOType::InOut || stgs.ioType == IOType::Out)
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

}

