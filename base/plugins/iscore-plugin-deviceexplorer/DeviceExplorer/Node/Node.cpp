#include "Node.hpp"

namespace iscore
{
QString DeviceExplorerNode::displayName() const
{
    switch(type())
    {
        case Type::Address:
            return get<AddressSettings>().name;
            break;
        case Type::Device:
            return get<DeviceSettings>().name;
            break;
        case Type::RootNode:
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
               ||  get<AddressSettings>().ioType == IOType::Invalid);
}

iscore::Node* getNodeFromString(iscore::Node* n, QStringList&& parts)
{
    auto theN = try_getNodeFromString(n, std::move(parts));
    Q_ASSERT(theN);
    return theN;
}

iscore::Node* try_getNodeFromString(iscore::Node* n, QStringList&& parts)
{
    if(parts.size() == 0)
        return n;

    for(const auto& child : n->children())
    {
        if(child->displayName() == parts[0])
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

    Q_ASSERT(n->is<DeviceSettings>());
    addr.device = n->get<DeviceSettings>().name;

    return addr;
}

}

